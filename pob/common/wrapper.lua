local callbackTable = { }
function SetCallback(name, func)
	callbackTable[name] = func
end
function GetCallback(name)
	return callbackTable[name]
end

function LoadModule(fileName, ...)
	if not fileName:match("%.lua") then
		fileName = fileName .. ".lua"
	end
	local func, err = loadfile(fileName)
	if func then
		return func(...)
	else
		error("LoadModule() error loading '"..fileName.."': "..err)
	end
end

function PLoadModule(fileName, ...)
	if not fileName:match("%.lua") then
		fileName = fileName .. ".lua"
	end
	local func, err = loadfile(fileName)
	if func then
		return PCall(func, ...)
	else
		error("PLoadModule() error loading '"..fileName.."': "..err)
	end
end

function ConPrintf(fmt, ...)
	print(string.format(fmt, ...))
end

function ConExecute(cmd)
end

function SetWindowTitle(title)
end

function RenderInit()
end

function GetScriptPath()
	return "/PathOfBuilding"
end

function GetRuntimePath()
	return "/"
end

function GetUserPath()
	return "/data"
end

local profiler = require("luatrace.profile")

function profiler_start()
    profiler.tron()
end

function profiler_stop()
    profiler.troff()
end

dofile("Launch.lua")
