return {
    Properties = {
        Speed = { Default = 5.0, Type = "Float" },
        Name = { Default = "Hero", Type = "String" },
        Test = { Default = Vec3(0, 5, 0), Type = "Vec3" },
        Image = { Type = "UIImage", Default = nil },
        Button = { Type = "UIButton", Default = nil },
        Text = { Type = "UIText", Default = nil }
    },

    OnCreate = function(self)
        print("Lua script created!")
        local transform = self.CurrentEntity.Transform
        if self.Image then
            self.Image:SetVisible(false)
            print("Image set invisible")
        end
        if self.Text then
            self.Text:SetText("Hello123!")
        end
        print("Initial Position: " .. transform.Translation.x .. ", " .. transform.Translation.y)
    end,

    OnUpdate = function(self, deltaTime)
        local transform = self.CurrentEntity.Transform
        
        local pos = transform.Translation
        pos.x = pos.x + deltaTime * self.Speed
        local moveY = Input.GetAxis("MoveUpDown")
        pos.z = pos.z + deltaTime * moveY * self.Speed
        
        transform.Translation = pos


        local rot = transform.Rotation
        rot.y = rot.y + deltaTime * 1.0
        transform.Rotation = rot
    end,

    OnDestroy = function(self)
        print("Lua script destroyed!")
    end
}