<#
.SYNOPSIS
    Downloads models and ground textures from Poly Haven and makes them usable by the engine.

.DESCRIPTION
    A Poly Haven model comes as one glTF file that can hold several objects standing side by side:
    three trees, seventeen clumps of grass. The engine loads a model file as one mesh, so this script
    writes one small .gltf per object, moved to the origin. They all share the download's one .bin and
    its textures, so no geometry is copied.

    The downloads also come without the alpha of leaves and twigs, which Poly Haven offers as a separate
    picture. It is stored in the alpha of the colour texture here, which becomes a PNG for it, and the
    materials that use it are set to cut-out.

    Every material of a download is marked two-sided, which is what Blender exports unless told otherwise.
    Bark and rock are closed surfaces whose inside is never seen, and drawing their far side costs a
    quarter to a third of the frame. So only materials with cut-outs stay two-sided.

    A ground texture, for the layers of a terrain, is three pictures: the colour, the normal map, and
    one that holds roughness in green and metalness in blue, which is how the engine's materials read
    them. They go into a folder of their own next to the models' folder.

    Files that are already there with the right size are not downloaded again.

    Neither models nor textures are kept in the repository: they are large, and this script makes them
    again. So that a benchmark still draws the same picture on every machine and next year, the hash of
    every file is recorded in a polyhaven-files.json next to them, which is kept in the repository.
    A file that comes out differently than recorded is reported. -Accept records the new files instead.

.EXAMPLE
    tools/fetch-polyhaven.ps1 -Asset fir_sapling_medium, grass_medium_01 -Texture rock_face_03
#>
param(
    [string[]]$Asset = @('fir_sapling_medium', 'fir_tree_01', 'pine_tree_01', 'grass_medium_01'),
    [string[]]$Texture = @('forest_ground_04', 'rock_face_03', 'brown_mud_leaves_01'),
    [string]$Resolution = '2k',
    [string]$Destination = (Join-Path $PSScriptRoot '..\samples\RenderBench\assets\Models'),
    [string]$TextureDestination = (Join-Path $PSScriptRoot '..\samples\RenderBench\assets\Textures'),

    # Leave the models, or the textures, as they are.
    [switch]$NoModels,
    [switch]$NoTextures,
    [switch]$Accept
)

$ErrorActionPreference = 'Stop'
$api = 'https://api.polyhaven.com'
$userAgent = 'model-fetch-script'

# The hashes that were recorded for the files of a folder, by their names.
function Read-Record([string]$Path)
{
    $known = @{}
    if (Test-Path $Path)
    {
        foreach ($entry in (Get-Content -Raw $Path | ConvertFrom-Json).PSObject.Properties)
        {
            $known[$entry.Name] = $entry.Value
        }
    }
    return $known
}

function Write-Record($Known, [string]$Path)
{
    if ($Known.Count -eq 0)
    {
        return
    }

    $sorted = [ordered]@{}
    foreach ($name in ($Known.Keys | Sort-Object))
    {
        $sorted[$name] = $Known[$name]
    }
    New-Item -ItemType Directory -Force (Split-Path $Path) | Out-Null
    [IO.File]::WriteAllText($Path, ($sorted | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))
}

if ($NoModels) { $Asset = @() }
if ($NoTextures) { $Texture = @() }

$Destination = [IO.Path]::GetFullPath($Destination)
$recordPath = Join-Path $Destination 'polyhaven-files.json'
$record = Read-Record $recordPath
$differing = 0

Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class AlphaMerge
{
    static byte[] Pixels(string path, out int width, out int height)
    {
        using (Bitmap source = new Bitmap(path))
        using (Bitmap bitmap = source.Clone(new Rectangle(0, 0, source.Width, source.Height), PixelFormat.Format32bppArgb))
        {
            width = bitmap.Width;
            height = bitmap.Height;
            BitmapData data = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            byte[] pixels = new byte[width * height * 4];
            Marshal.Copy(data.Scan0, pixels, 0, pixels.Length);
            bitmap.UnlockBits(data);
            return pixels;
        }
    }

    // Writes the colour picture with the grey of the alpha picture as its alpha, as a PNG.
    // Returns the smallest alpha. Writes nothing if the alpha picture is white all over,
    // because then nothing is cut out and the colour texture can stay as it is.
    public static int Merge(string colourPath, string alphaPath, string outputPath)
    {
        int width, height, alphaWidth, alphaHeight;
        byte[] colour = Pixels(colourPath, out width, out height);
        byte[] alpha = Pixels(alphaPath, out alphaWidth, out alphaHeight);
        if (width != alphaWidth || height != alphaHeight)
            throw new InvalidOperationException("The colour and the alpha picture differ in size.");

        int smallest = 255;
        for (int i = 0; i < colour.Length; i += 4)
        {
            // Stored as blue, green, red, alpha. The alpha picture is grey, so any colour channel will do.
            byte value = alpha[i + 2];
            colour[i + 3] = value;
            if (value < smallest)
                smallest = value;
        }

        if (smallest >= 250)
            return smallest;

        using (Bitmap result = new Bitmap(width, height, PixelFormat.Format32bppArgb))
        {
            BitmapData data = result.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(colour, 0, data.Scan0, colour.Length);
            result.UnlockBits(data);
            result.Save(outputPath, ImageFormat.Png);
        }
        return smallest;
    }
}
'@

function Get-RemoteFile([string]$Url, [string]$Path, [long]$Size)
{
    if ((Test-Path $Path) -and (Get-Item $Path).Length -eq $Size)
    {
        return
    }

    New-Item -ItemType Directory -Force (Split-Path $Path) | Out-Null
    Write-Host ("  {0} ({1:N0} MB)" -f (Split-Path $Path -Leaf), ($Size / 1MB))
    & curl.exe --location --fail --silent --show-error --user-agent $userAgent --output $Path $Url
    if ($LASTEXITCODE -ne 0)
    {
        throw "Download failed: $Url"
    }
}

function Write-Gltf($Gltf, [string]$Path)
{
    # Without a byte order mark, which glTF does not allow.
    [IO.File]::WriteAllText($Path, ($Gltf | ConvertTo-Json -Depth 100), (New-Object Text.UTF8Encoding $false))
}

# Stores the alpha Poly Haven keeps apart in the colour textures and sets their materials to cut-out.
function Merge-Alpha($Gltf, $Files, [string]$Id, [string]$Folder)
{
    for ($image = 0; $image -lt @($Gltf.images).Count; $image++)
    {
        $uri = $Gltf.images[$image].uri
        if ($uri -notmatch "^textures/${Id}_(.*)diff_${Resolution}\.jpg$")
        {
            continue
        }

        # "twig_diff" goes with "twig_alpha", a plain "diff" with "Alpha".
        $alphaName = "$($Matches[1])alpha"
        $alphaMap = $Files.PSObject.Properties | Where-Object { $_.Name -ieq $alphaName } | Select-Object -First 1
        if (-not $alphaMap)
        {
            continue
        }

        $colourPath = Join-Path $Folder $uri
        $mergedUri = $uri -replace '\.jpg$', '.png'

        # Without the JPEG, an earlier run has merged this one already.
        if (Test-Path $colourPath)
        {
            $alphaFile = $alphaMap.Value.$Resolution.png
            $alphaPath = Join-Path $Folder "alpha_$image.png"
            Get-RemoteFile $alphaFile.url $alphaPath $alphaFile.size

            $smallest = [AlphaMerge]::Merge($colourPath, $alphaPath, (Join-Path $Folder $mergedUri))
            Remove-Item $alphaPath
            if ($smallest -ge 250)
            {
                Write-Host "  $alphaName is white all over, $uri stays as it is"
                continue
            }
            Remove-Item $colourPath
        }

        $Gltf.images[$image].uri = $mergedUri
        if ($Gltf.images[$image].PSObject.Properties['mimeType'])
        {
            $Gltf.images[$image].mimeType = 'image/png'
        }

        foreach ($material in $Gltf.materials)
        {
            $colourTexture = $material.pbrMetallicRoughness.baseColorTexture
            if ($null -eq $colourTexture -or $Gltf.textures[$colourTexture.index].source -ne $image)
            {
                continue
            }

            $material | Add-Member -Force -NotePropertyName alphaMode -NotePropertyValue 'MASK'
            $material | Add-Member -Force -NotePropertyName alphaCutoff -NotePropertyValue 0.5
            Write-Host "  $($material.name): cut-out, alpha from $alphaName"
        }
    }
}

function Get-TriangleCount($Gltf, $Node)
{
    $count = 0
    if ($null -ne $Node.PSObject.Properties['mesh'])
    {
        foreach ($primitive in $Gltf.meshes[$Node.mesh].primitives)
        {
            $count += $Gltf.accessors[$primitive.indices].count / 3
        }
    }
    if ($null -ne $Node.PSObject.Properties['children'])
    {
        foreach ($child in $Node.children)
        {
            $count += Get-TriangleCount $Gltf $Gltf.nodes[$child]
        }
    }
    return $count
}

# Marks the materials without cut-outs one-sided. Call it after Merge-Alpha, which decides what has cut-outs.
function Set-Sides($Gltf)
{
    foreach ($material in $Gltf.materials)
    {
        $alphaMode = $material.PSObject.Properties['alphaMode']
        if ($null -ne $alphaMode -and $alphaMode.Value -ne 'OPAQUE')
        {
            continue
        }
        $material | Add-Member -Force -NotePropertyName doubleSided -NotePropertyValue $false
    }
}

# Writes one .gltf per object of the file. Each is the whole file with a scene that holds that object only.
function Split-Objects($Gltf, [string]$Id, [string]$Folder)
{
    $sceneIndex = 0
    if ($null -ne $Gltf.PSObject.Properties['scene'])
    {
        $sceneIndex = $Gltf.scene
    }
    $roots = @($Gltf.scenes[$sceneIndex].nodes)
    $json = $Gltf | ConvertTo-Json -Depth 100

    foreach ($root in $roots)
    {
        $single = $json | ConvertFrom-Json
        $node = $single.nodes[$root]
        if ($null -ne $node.PSObject.Properties['matrix'])
        {
            throw "Node $($node.name) is placed by a matrix, which this script cannot move to the origin."
        }

        # Where the object stood next to the others. Its turn and size are kept.
        $node.PSObject.Properties.Remove('translation')

        $single.scenes = @([pscustomobject]@{ name = $node.name; nodes = @($root) })
        $single | Add-Member -Force -NotePropertyName scene -NotePropertyValue 0

        $name = $Id
        if ($roots.Count -gt 1)
        {
            $name = $node.name -replace '_LOD0$', ''
        }
        Write-Gltf $single (Join-Path $Folder "$name.gltf")
        Write-Host ("  {0}.gltf: {1:N0} triangles" -f $name, (Get-TriangleCount $single $node))
    }
}

# Compares the files of a model with what was recorded when they were first made, and records new ones.
function Test-Files([string]$Id, [string]$Folder)
{
    # The engine writes an .lxmeta next to every asset it has seen. Those are not part of the model.
    foreach ($file in Get-ChildItem $Folder -Recurse -File | Where-Object Extension -ne '.lxmeta')
    {
        $name = "$Id/" + $file.FullName.Substring($Folder.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash $file.FullName -Algorithm SHA256).Hash
        if ($Accept -or -not $record.ContainsKey($name))
        {
            $record[$name] = $hash
        }
        elseif ($record[$name] -ne $hash)
        {
            Write-Warning "$name is not the file that was recorded."
            $script:differing++
        }
    }
}

foreach ($id in $Asset)
{
    Write-Host $id
    $folder = Join-Path $Destination $id
    $files = Invoke-RestMethod "$api/files/$id" -UserAgent $userAgent
    $download = $files.gltf.$Resolution.gltf

    # The file as Poly Haven makes it is only read here. What the engine loads are the files made from it.
    $sourcePath = Join-Path $folder "$id.source.json"
    Get-RemoteFile $download.url $sourcePath $download.size
    foreach ($included in $download.include.PSObject.Properties)
    {
        $path = Join-Path $folder $included.Name
        $merged = $path -replace '\.jpg$', '.png'
        if ($included.Name -match 'diff' -and (Test-Path $merged))
        {
            # Already downloaded and given its alpha in an earlier run.
            continue
        }
        Get-RemoteFile $included.Value.url $path $included.Value.size
    }

    $gltf = Get-Content -Raw $sourcePath | ConvertFrom-Json
    Remove-Item $sourcePath

    Merge-Alpha $gltf $files $id $folder
    Set-Sides $gltf
    Split-Objects $gltf $id $folder
    Test-Files $id $folder
}

Write-Record $record $recordPath

# The ground textures: the three pictures of each, as they are.
$TextureDestination = [IO.Path]::GetFullPath($TextureDestination)
$textureRecordPath = Join-Path $TextureDestination 'polyhaven-files.json'
$record = Read-Record $textureRecordPath
foreach ($id in $Texture)
{
    Write-Host $id
    $folder = Join-Path $TextureDestination $id
    $files = Invoke-RestMethod "$api/files/$id" -UserAgent $userAgent
    foreach ($map in 'Diffuse', 'nor_gl', 'arm')
    {
        $download = $files.$map.$Resolution.jpg
        if ($null -eq $download)
        {
            throw "$id has no $map picture at $Resolution."
        }
        Get-RemoteFile $download.url (Join-Path $folder (Split-Path $download.url -Leaf)) $download.size
    }
    Test-Files $id $folder
}
Write-Record $record $textureRecordPath

if ($differing -gt 0)
{
    Write-Warning "$differing files differ from what was recorded. Poly Haven may have changed a model or a texture, and benchmark pictures may change with it. Run again with -Accept to record the new files."
    exit 1
}
