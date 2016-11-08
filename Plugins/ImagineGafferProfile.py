
from PluginAPI.BaseGafferProfile import Profile

class ImagineProfile(Profile):
	class Constants:
		SHADER_COLOR_PARAM_NAMES = ('imagineLightParams.colour',)
		SHADER_INT_PARAM_NAMES = ('imagineLightParams.intensity',)
		SHADER_EXP_PARAM_NAMES = ('imagineLightParams.exposure',)

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
