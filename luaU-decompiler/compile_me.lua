--local alal = { a, { a }, a = p, alal, lala, {alal, lala, {alal, lala, {alal, lala} } } , appa = {alal, lala, {alal, lala, {alal, lala, {alal, lala, appa = {alal, lala, {alal, lala, {alal, lala, {alal, lala, appa = {alal, lala, {alal, lala, {alal, lala, {alal, lala}}}}}}}}}}}}   }
--print (alal)

--local function  breun (fallam, falastina) 
--
----local aa = aoa or jaja and aja or jaja or aja or aoa or jaja or aja;
--if ( aa or aa and aja == 10 or  aoa or jaja or aja or aoa or jaja or aja) then
--   pr()
--end
--
--print (aa)
--
--while penis do
--    
--    print ("ALLAH")
--    if (aoo == 10 or aii == 100) then
--        break;
--    end
--
--end
-- --
--end
--
--
--breun(aa:breun());

-- Metas
-- udachio

-- Gui Functions
local function getResource(name)
	return resources:WaitForChild(name):Clone()
end


function f.buildPanes()
	--print("\n-----\n")
	--for i,v in pairs(RPaneItems) do print(v.Window) end
	--print("\n-----")
	
	for i,v in pairs(RPaneItems) do
		v.Window:TweenSizeAndPosition(UDim2.new(0,explorerSettings.RPaneWidth,v.Proportion,0),UDim2.new(0,0,f.prevProportions(RPaneItems,i-1),0),Enum.EasingDirection.Out,Enum.EasingStyle.Quart,0.5,true)
		--v.Window.Position = UDim2.new(0,0,prevProportions(RPaneItems,i-1),0)
		--v.Window.Size = UDim2.new(0,explorerSettings.RPaneWidth,v.Proportion,0)
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

function f.removeFromPane(window)
	local pane
	local windowIndex
	
	for i,v in pairs(LPaneItems) do if v.Window == window then pane = LPaneItems windowIndex = i end end
	for i,v in pairs(RPaneItems) do if v.Window == window then pane = RPaneItems windowIndex = i end end	
	
	if pane and #pane > 0 then
		local weightTop,weightBottom,weightTopN,weightBottomN = 0,0			
		
		for i = windowIndex-1,1,-1 do weightTop = weightTop + RPaneItems[i].Proportion end	
		for i = windowIndex+1,#RPaneItems do weightBottom = weightBottom + RPaneItems[i].Proportion end	
		
		if weightTop > 0 and weightBottom == 0 then
			weightTopN = weightTop + RPaneItems[windowIndex].Proportion
		elseif weightTop == 0 and weightBottom > 0 then
			weightBottomN = weightBottom + RPaneItems[windowIndex].Proportion
		else
			weightTopN = weightTop + RPaneItems[windowIndex].Proportion/2
			weightBottomN = weightBottom + RPaneItems[windowIndex].Proportion/2
		end
			
		for i = 1,windowIndex-1 do
			RPaneItems[i].Proportion = RPaneItems[i].Proportion / weightTop * weightTopN
		end
		for i = windowIndex+1,#RPaneItems do
			RPaneItems[i].Proportion = RPaneItems[i].Proportion / weightBottom * weightBottomN
		end

		table.remove(RPaneItems,windowIndex)
		f.buildPanes()
	end
end

function f.resizePaneItem(window,pane,size)
	local windowIndex = 0
	local sizeWeight = 0
	size = math.max(0.2,size)
	if pane == "Right" then
		for i,v in pairs(RPaneItems) do
			if v.Window == window then windowIndex = i break end
		end
			
		for i = windowIndex+1,#RPaneItems do
			sizeWeight = sizeWeight + RPaneItems[i].Proportion
		end
		
		local oldSize = 1-(sizeWeight+RPaneItems[windowIndex].Proportion)
		
		RPaneItems[windowIndex].Proportion = size
		
		for i = 1,windowIndex-1 do
			RPaneItems[i].Proportion = RPaneItems[i].Proportion / oldSize * (1-(sizeWeight+size))
		end
		
		for i,v in pairs(RPaneItems) do
			print(v.Window, v.Proportion)
		end
	end
	f.buildPanes()
end


f.fetchRMD = function()
	local rawRMD = nil
    if script and script:FindFirstChild("RMD") then
        rawRMD = require(script.RMD)
    end
    rawRMD = Services.HttpService:JSONDecode(rawRMD)

	local RMD = {}
	for _,v in pairs(rawRMD) do
		RMD[v.Name] = v
	end

    return RMD
end
