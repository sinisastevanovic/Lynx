return {
    OnCreate = function(self)
        print("Lua script created!")
        local transform = self.CurrentEntity.Transform
        print("Initial Position: " .. transform.Translation.x .. ", " .. transform.Translation.y)
    end,

    OnUpdate = function(self, deltaTime)
        local transform = self.CurrentEntity.Transform
        
        local pos = transform.Translation
        pos.x = pos.x + deltaTime * 0.5
        transform.Translation = pos


        local rot = transform.Rotation
        rot.y = rot.y + deltaTime * 1.0
        transform.Rotation = rot
    end,

    OnDestroy = function(self)
        print("Lua script destroyed!")
    end
}