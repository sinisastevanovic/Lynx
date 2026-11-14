return {
    Properties = {
        TargetArmLength = { Default = 15.0, Type = "Float" },
        TargetOffset = { Default = Vec3(0.0, 1.5, 0.0), Type = "Vec3" },
        Pitch = { Default = 20.0, Type = "Float" },
        Yaw = { Default = 0.0, Type = "Float" },
        Sensitivity = { Default = Vec2(0.15, 0.15), Type = "Vec2" },
        MinMaxPitch = { Default = Vec2(-10, 85), Type = "Vec2" },
        LastMousePosition = { Default = Vec2(0, 0), Type = "Vec2" },
        IsFirstUpdate = { Default = true, Type = "Bool" }
    },

    OnCreate = function(self)
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
    end
}