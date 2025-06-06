#ifndef FSWEATHER_IS_INCLUDED
#define FSWEATHER_IS_INCLUDED
/* { */

#include "fsdef.h"

// FS_FOG_VISIBILITY_MAX, FS_FOG_VISIBILITY_MIN, FSCLOUDLAYER_NONE, and FSCLOUDLAYER_OVERCAST are moved to fsdef.h


class FsWeatherCloudLayer
{
public:
	int cloudLayerType;
	double y0,y1;

	static const char *CloudLayerTypeString(int cloudLayerType);
	static int CloudLayerTypeFromString(const char str[]);
};



class FsWeather
{
protected:
	YsVec3 wind;
	YSBOOL fog;
	double fogVisibility;
	FSWEATHERTYPE weatherType;

	YsVec3 transWind;
	double transFogVisibility;

	YsArray <FsWeatherCloudLayer> cloudLayer;
	
	// Rain-related properties
	YSBOOL isRaining;
	double rainIntensity;
	double thunderTimer;
	YsVec3 skyColor;
	YsVec3 fogColor;

public:
	FsWeather();
	~FsWeather();

	void WeatherTransition(const double &dt);

	const YsVec3 &GetWind(void) const;
	YSRESULT SetWind(const YsVec3 &wind);
	YSRESULT SetTransWind(const YsVec3 &wind);

	const double &GetFogVisibility(void) const;
	YSRESULT SetFogVisibility(const double &visibility);
	YSRESULT SetTransFogVisibility(const double &visibility);

	void SetFog(YSBOOL f);
	void SetTransFog(YSBOOL f);
	YSBOOL GetFog(void) const;

	void SetWeatherType(FSWEATHERTYPE type);
	FSWEATHERTYPE GetWeatherType(void) const;
	YSBOOL IsRaining(void) const;
	double GetRainIntensity(void) const;
	const YsVec3 &GetSkyColor(void) const;
	const YsVec3 &GetFogColor(void) const;
	void UpdateRain(const double &dt);
	void DrawRain(const YsVec3 &cameraPos, const YsVec3 &cameraDir) const;
	void DrawRainWithTerrain(const YsVec3 &cameraPos, const YsVec3 &cameraDir, const class FsSimulation *sim) const;

	void SetCloudLayer(YSSIZE_T nLayer,const FsWeatherCloudLayer layer[]);
	void AddCloudLayer(const FsWeatherCloudLayer &layer);
	void GetCloudLayer(int &nLayer,const FsWeatherCloudLayer *&layer) const;
	YSBOOL IsInCloudLayer(const YsVec3 &pos) const;

	YSRESULT Save(FILE *fp) const;
	YSRESULT Load(FILE *fp);

	void DrawCloudLayer(const YsVec3 &cameraPos) const;
};


/* } */
#endif
