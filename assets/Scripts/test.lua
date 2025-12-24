return {
    Properties = {
        Speed = { Default = 5.0, Type = "Float" },
        Name = { Default = "Hero", Type = "String" },
        Test = { Default = Vec3(0, 5, 0), Type = "Vec3" }
    },

    OnCreate = function(self)
        print("Lua script created!")
        local transform = self.CurrentEntity.Transform
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