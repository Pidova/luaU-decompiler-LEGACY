
function f.buildPanes()

	for i,v in pairs(RPaneItems) do
		v.Window:TweenSizeAndPosition(UDim2.new(0,explorerSettings.RPaneWidth,v.Proportion,0),UDim2.new(0,0,f.prevProportions(RPaneItems,i-1),0),Enum.EasingDirection.Out,Enum.EasingStyle.Quart,0.5,true)
	end
end

function f.distance(x1,y1,x2,y2)
	return math.sqrt((x2-x1)^2+(y2-y1)^2)
end

function f.checkMouseInGui(gui)
	if gui == nil then return false end
	local guiPosition = gui.AbsolutePosition
	local guiSize = gui.AbsoluteSize	
	
	if mouse.X >= guiPosition.x and mouse.X <= guiPosition.x + guiSize.x and mouse.Y >= guiPosition.y and mouse.Y <= guiPosition.y + guiSize.y then
		return true
	else
		return false
	end
end

function f.addToPane(window,pane)
	if pane == "Right" then
		for i,v in pairs(RPaneItems) do if v.Window == window then return end end
		for i,v in pairs(RPaneItems) do
			RPaneItems[i].Proportion = v.Proportion / 100 * 80
		end
		window.Parent = contentR
		if #RPaneItems == 0 then
			table.insert(RPaneItems,{Window = window, Proportion = 1})
		else
			table.insert(RPaneItems,{Window = window, Proportion = 0.2})
		end
	end
	f.buildPanes()
end