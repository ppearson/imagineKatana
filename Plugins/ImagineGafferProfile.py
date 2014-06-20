
from PluginAPI.BaseGafferProfile import Profile

class ImagineProfile(Profile):
	class Constants:
		SHADER_COLOR_PARAM_NAMES = ('imagineLightParams.color',)
		SHADER_INTENSITY_PARAM_NAMES = ('imagineLightParams.intensity',)
	
	def getLightShaderType(self):
		return 'light'
	
	def getMaterialParameterName(self):
		return 'imagineLightShader'
	
	def getMaterialParameterValue(self):
		return 'imagine.light'
	
	def getRendererName(self):
		return 'imagine'
	
PluginRegistry = [
	("GafferProfile", 1, "imagine", ImagineProfile)
]
