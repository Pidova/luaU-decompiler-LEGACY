
local function addDivider(self)
	table.insert(self.Items,"Divider")
end
newMt.AddDivider = addDivider

local function clear(self)
	self.Items = {}
end
newMt.Clear = clear

local function refresh(self)
	mainFrame:ClearAllChildren()
	
	local currentPos = 2
	for _,item in pairs(self.Items) do
		if item == "Divider" then
			local newDivider = dividerFrame:Clone()
			newDivider.Position = UDim2.new(0,0,0,currentPos)
			newDivider.Parent = mainFrame
			currentPos = currentPos + 12
		else
			local newEntry = entryFrame:Clone()
			newEntry.Position = UDim2.new(0,0,0,currentPos)
			newEntry.EntryName.Text = item.Name
			newEntry.Shortcut.Text = item.Shortcut
			if item.Disabled then
				newEntry.EntryName.TextColor3 = Color3.new(150/255,150/255,150/255)
				newEntry.Shortcut.TextColor3 = Color3.new(150/255,150/255,150/255)
			end
			
			local useIcon = item.Disabled and item.DisabledIcon or item.Icon
			if type(useIcon) == "string" then
				newEntry.IconFrame.Icon.Image = useIcon
			else
				newEntry.IconFrame:Destroy()
				local newIcon = useIcon:Clone()
				newIcon.Position = UDim2.new(0,2,0.5,-8)
				newIcon.Parent = newEntry
			end
			
			if item.OnClick and not item.Disabled then newEntry.MouseButton1Click:Connect(item.OnClick) end
			
			newEntry.InputBegan:Connect(function(input)
				if input.UserInputType == Enum.UserInputType.MouseMovement then
					newEntry.BackgroundTransparency = 0.5
				end
			end)
			
			newEntry.InputEnded:Connect(function(input)
				if input.UserInputType == Enum.UserInputType.MouseMovement then
					newEntry.BackgroundTransparency = 1
				end
			end)
			
			newEntry.Parent = mainFrame
			currentPos = currentPos + self.Height
		end
	end
	
	mainFrame.Size = UDim2.new(0,self.Width,0,currentPos+2)
end
newMt[1].Refresh = refresh

local function show(self,displayFrame,x,y)
	local toSize = mainFrame.Size.Y.Offset
	local reverseY = false
	
	local maxX,maxY = gui.AbsoluteSize.X,gui.AbsoluteSize.Y
	
	if x + self.Width > maxX then x = x - self.Width end
	if y + toSize > maxY then reverseY = true end
	
	mainFrame.Position = UDim2.new(0,x,0,y)
	mainFrame.Size = UDim2.new(0,self.Width,0,0)
	mainFrame.Parent = displayFrame
	
	local closeEvent = Services.UserInputService.InputBegan:Connect(function(input)
		if input.UserInputType ~= Enum.UserInputType.MouseButton1 then return end
		
		if not f.checkMouseInGui(mainFrame) then
			self:Hide()
		end
	end)
	
	if reverseY then
		if y - toSize < 0 then y = toSize end
		mainFrame:TweenSizeAndPosition(UDim2.new(0,self.Width,0,toSize),UDim2.new(0,x,0,y - toSize),Enum.EasingDirection.Out,Enum.EasingStyle.Quart,0.2,true)
	else
		mainFrame:TweenSize(UDim2.new(0,self.Width,0,toSize),Enum.EasingDirection.Out,Enum.EasingStyle.Quart,0.2,true)
	end
end

show();