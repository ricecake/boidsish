#ifndef CLOUD_WEATHER_TEXTURE_DEFINED
#define CLOUD_WEATHER_TEXTURE_DEFINED
layout(binding = [[CLOUD_WEATHER_BINDING]]) uniform sampler2D u_cloudWeatherTexture;
#endif

#ifndef CLOUD_2D_PROPS_TEXTURE_DEFINED
#define CLOUD_2D_PROPS_TEXTURE_DEFINED
layout(binding = [[CLOUD_2D_PROPS_BINDING]]) uniform sampler2D u_cloud2DPropsLUT;
#endif

#ifndef CLOUD_3D_FRONT_TEXTURE_DEFINED
#define CLOUD_3D_FRONT_TEXTURE_DEFINED
layout(binding = [[CLOUD_3D_FRONT_BINDING]]) uniform sampler3D u_cloud3DFrontLUT;
#endif
