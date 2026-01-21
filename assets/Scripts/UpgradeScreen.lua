return {
    Properties = {
        Container = { Type = "UIElement", Default = nil },
        Card1_Title = { Type = "UIText", Default = nil },
        Card1_Desc = { Type = "UIText", Default = nil },
        Card1_Btn = { Type = "UIButton", Default = nil },
        Card2_Title = { Type = "UIText", Default = nil },
        Card2_Desc = { Type = "UIText", Default = nil },
        Card2_Btn = { Type = "UIButton", Default = nil },
        Card3_Title = { Type = "UIText", Default = nil },
        Card3_Desc = { Type = "UIText", Default = nil },
        Card3_Btn = { Type = "UIButton", Default = nil }
    },

    Upgrades = {
        { Name = "Spinach", Desc = "Dmg +10%", Stat = "Might", Val = 0.1 },
        { Name = "Wings", Desc = "Speed +10%", Stat = "MoveSpeed", Val = 0.5 },
        { Name = "Armor", Desc = "Armor +1", Stat = "Armor", Val = 1.0 }
    },

    OnCreate = function(self)
        if self.Container then
            self.Container:SetVisible(false)
        end

        if self.Card1_Btn then
            self.Card1_Btn.OnClick = function()
                self:ApplyUpgrade(1)
            end
        end
        if self.Card2_Btn then
            self.Card2_Btn.OnClick = function()
                self:ApplyUpgrade(3)
            end
        end
        if self.Card3_Btn then
            self.Card3_Btn.OnClick = function()
                self:ApplyUpgrade(3)
            end
        end
    end,

    OnLevelUp = function(self, player, level)
        print("Level Up! " .. level)
        Game.SetPaused(true)

        self.Container:SetVisible(true)

        local setup = function(title, desc, btn, upgrade)
            title:SetText(upgrade.Name)
            desc:SetText(upgrade.Desc)
            btn.OnClick = function()
                self:ApplyUpgrade(player, upgrade)
            end
        end

        setup(self.Card1_Title, self.Card1_Desc, self.Card1_Btn, self.Upgrades[1])
        setup(self.Card2_Title, self.Card2_Desc, self.Card2_Btn, self.Upgrades[2])
        setup(self.Card3_Title, self.Card3_Desc, self.Card3_Btn, self.Upgrades[3])
    end,

    ApplyUpgrade = function(self, player, upgrade)
        print("Selected Upgrade " .. upgrade.Name)
        self.Container:SetVisible(false)
        Game.SetPaused(false)
    end
}