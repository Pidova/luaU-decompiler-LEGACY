ScrollMt = {
    __index =
    {
        AddMarker = function(self, ind, color)
            self.Markers[ind] = color or Color3.new(0, 0, 0)
        end,
        ScrollTo = function(self, ind)
            self.Index = ind
            self:Update()
        end,
        ScrollUp = function(self)
            self.Index = self.Index - self.Increment
            self:Update()
        end,
        ScrollDown = function(self)
            self.Index = self.Index + self.Increment
            self:Update()
        end,
        CanScrollUp = function(self)
            return self.Index > 0
        end,
        CanScrollDown = function(self)
            return self.Index + self.VisibleSpace < self.TotalSpace
        end,
        GetScrollPercent = function(self)
            return self.Index /(self.TotalSpace - self.VisibleSpace)
        end,
        SetScrollPercent = function(self, perc)
            self.Index = math.floor(perc *(self.TotalSpace - self.VisibleSpace))
            self:Update()
        end
    }
}