#include <cstddef>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <time.h>
#include <vector>
#include <algorithm>

#include <ysclass.h>
#include <ysgl.h>

#include "fs.h"

#ifndef YsPi
#define YsPi 3.14159265358979323846
#endif

// Rain particle structure for realistic physics
struct RainParticle
{
    YsVec3 position;
    YsVec3 velocity;
    float life;
    float maxLife;
    bool hitGround;
    YsVec3 splashPos;
    float splashLife;
    float size;
    bool active;
    float distanceFromCamera;

    RainParticle() : life(0.0f), maxLife(3.0f), hitGround(false), splashLife(0.0f), size(1.0f), active(false), distanceFromCamera(1000.0f) {}
};

// Ground wetness effect structure
struct WetnessPoint
{
    YsVec3 position;
    float intensity;
    float life;
    float maxLife;
    bool active;

    WetnessPoint() : intensity(1.0f), life(0.0f), maxLife(10.0f), active(false) {}
};

// Global rain particle system
static std::vector<RainParticle> rainParticles;
static std::vector<WetnessPoint> wetnessPoints;
static bool particleSystemInitialized = false;
static const int MAX_RAIN_PARTICLES = 2000;
static const int MAX_WETNESS_POINTS = 200;
static double lastGroundCheck = 0.0;
static double lastOptimizationTime = 0.0;

const char *FsWeatherCloudLayer::CloudLayerTypeString(int cloudLayerType)
{
	switch(cloudLayerType)
	{
	default:
	case FSCLOUDLAYER_NONE:
		return "CLR";
	case FSCLOUDLAYER_OVERCAST:
		return "OVC";
	};
}

int FsWeatherCloudLayer::CloudLayerTypeFromString(const char str[])
{
	if(strcmp(str,"OVC")==0 || strcmp(str,"ovc")==0)
	{
		return FSCLOUDLAYER_OVERCAST;
	}
	return FSCLOUDLAYER_NONE;
}

FsWeather::FsWeather()
{
	wind.Set(0.0,0.0,0.0);
	fog=YSTRUE;
	fogVisibility=20000;
	weatherType=FSWEATHER_CLEAR;

	transWind.Set(0.0,0.0,0.0);
	transFogVisibility=20000;

	// Initialize rain properties
	isRaining=YSFALSE;
	rainIntensity=0.0;
	thunderTimer=0.0;
	skyColor.Set(0.5, 0.7, 1.0);  // Default clear sky blue
	fogColor.Set(0.8, 0.8, 0.8);  // Default light grey fog
}

FsWeather::~FsWeather()
{
}

void FsWeather::WeatherTransition(const double &dt)
{
	if(wind!=transWind)
	{
		YsVec3 d;
		double l,chg;

		chg=4.0*dt;

		d=transWind-wind;
		l=d.GetLength();
		if(l<chg)
		{
			wind=transWind;
		}
		else
		{
			d/=l;
			wind+=d*chg;
		}
	}

	if(YsEqual(fogVisibility,transFogVisibility)!=YSTRUE)
	{
		double d,chg;

		d=transFogVisibility-fogVisibility;
		chg=10000.0*dt;

		if(fabs(d)<chg)
		{
			fogVisibility=transFogVisibility;
		}
		else
		{
			if(d>0)
			{
				fogVisibility+=chg;
			}
			else
			{
				fogVisibility-=chg;
			}
		}
	}
}

const YsVec3 &FsWeather::GetWind(void) const
{
	return wind;
}

YSRESULT FsWeather::SetWind(const YsVec3 &w)
{
	wind=w;
	transWind=w;
	return YSOK;
}

YSRESULT FsWeather::SetTransWind(const YsVec3 &w)
{
	transWind=w;
	return YSOK;
}

const double &FsWeather::GetFogVisibility(void) const
{
	return fogVisibility;
}

YSRESULT FsWeather::SetFogVisibility(const double &visibility)
{
	fogVisibility=YsBound(visibility,FS_FOG_VISIBILITY_MIN,FS_FOG_VISIBILITY_MAX);
	transFogVisibility=fogVisibility;
	return YSOK;
}

YSRESULT FsWeather::SetTransFogVisibility(const double &vis)
{
	transFogVisibility=vis;
	return YSOK;
}

void FsWeather::SetFog(YSBOOL f)
{
	fog=f;
}

YSBOOL FsWeather::GetFog(void) const
{
	return fog;
}

void FsWeather::SetCloudLayer(YSSIZE_T nLayer,const FsWeatherCloudLayer layer[])
{
	cloudLayer.Set(nLayer,layer);
}

void FsWeather::AddCloudLayer(const FsWeatherCloudLayer &layer)
{
	cloudLayer.Append(layer);
}

void FsWeather::GetCloudLayer(int &nLayer,const FsWeatherCloudLayer *&layer) const
{
	nLayer=(int)cloudLayer.GetN();
	layer=cloudLayer;
}

YSBOOL FsWeather::IsInCloudLayer(const YsVec3 &pos) const
{
	int i;
	forYsArray(i,cloudLayer)
	{
		if(cloudLayer[i].y0<=pos.y() && pos.y()<=cloudLayer[i].y1)
		{
			return YSTRUE;
		}
	}
	return YSFALSE;
}

YSRESULT FsWeather::Save(FILE *fp) const
{
	fprintf(fp,"WEATHERX\n");

	fprintf(fp,"CONSTWIND %lfm/s %lfm/s %lfm/s\n",wind.x(),wind.y(),wind.z());
	fprintf(fp,"VISIBILIT %lfm\n",fogVisibility);

	int i;
	forYsArray(i,cloudLayer)
	{
		fprintf(fp,"CLOUDLAYER %s %lfm %lfm\n",
		    FsWeatherCloudLayer::CloudLayerTypeString(cloudLayer[i].cloudLayerType),
		    cloudLayer[i].y0,
		    cloudLayer[i].y1);
	}

	fprintf(fp,"ENDWEATHER\n");

	return YSOK;
}

static const char *const weatherCmd[]=
{
	"ENDWEATHER",
	"CONSTWIND",
	"VISIBILIT",
	"CLOUDLAYER",
	NULL
};

YSRESULT FsWeather::Load(FILE *fp)
{
	// Keyword WEATHER is already read by FsWorld
	char buf[256];
	int ac;
	char *av[16];

	while(fgets(buf,256,fp)!=NULL)
	{
		if(YsArguments(&ac,av,16,buf)==YSOK)
		{
			int cmd;
			if(YsCommandNumber(&cmd,av[0],weatherCmd)==YSOK)
			{
				switch(cmd)
				{
				case 0:  // ENDWEATHER
					goto ENDWEATHER;
				case 1:  // CONSTWIND
					if(ac>=4)
					{
						double x,y,z;
						if(FsGetSpeed(x,av[1])==YSOK &&
						   FsGetSpeed(y,av[2])==YSOK &&
						   FsGetSpeed(z,av[3])==YSOK)
						{
							wind.Set(x,y,z);
							transWind.Set(x,y,z);  // 2009/04/28
						}
					}
					break;
				case 2:  // VISIBILIT
					if(ac>=2)
					{
						double l;
						if(FsGetLength(l,av[1])==YSOK)
						{
							fogVisibility=l;
							transFogVisibility=l;  // 2009/04/28
						}
					}
					break;
				case 3: // 	"CLOUDLAYER",
					if(ac>=4)
					{
						FsWeatherCloudLayer lyr;
						lyr.cloudLayerType=FsWeatherCloudLayer::CloudLayerTypeFromString(av[1]);
						FsGetLength(lyr.y0,av[2]);
						FsGetLength(lyr.y1,av[3]);
						cloudLayer.Append(lyr);
					}
					break;
				}
			}
		}
	}

ENDWEATHER:
	return YSOK;
}

void FsWeather::SetWeatherType(FSWEATHERTYPE type)
{
	weatherType = type;
	if(type == FSWEATHER_RAIN)
	{
		isRaining = YSTRUE;
		rainIntensity = 0.9;
		// Much darker, more atmospheric rain colors
		skyColor.Set(0.08, 0.12, 0.18);  // Very dark stormy sky
		fogColor.Set(0.15, 0.18, 0.22);  // Dark atmospheric fog


		// Add default storm cloud layers if none exist
		if(cloudLayer.GetN() == 0)
		{
			FsWeatherCloudLayer stormClouds;
			stormClouds.cloudLayerType = FSCLOUDLAYER_OVERCAST;
			stormClouds.y0 = 40000.0;   // Lower cloud base
			stormClouds.y1 = 80000.0;   // Higher cloud top for thicker clouds
			cloudLayer.Append(stormClouds);
		}
	}
	else
	{
		isRaining = YSFALSE;
		rainIntensity = 0.0;
		skyColor.Set(0.5, 0.7, 1.0);  // Clear blue sky
		fogColor.Set(0.8, 0.8, 0.8);  // Light grey fog

		// Clear cloud layers for clear weather
		cloudLayer.Set(0, NULL);
	}
}

FSWEATHERTYPE FsWeather::GetWeatherType(void) const
{
	return weatherType;
}

YSBOOL FsWeather::IsRaining(void) const
{
	return isRaining;
}

double FsWeather::GetRainIntensity(void) const
{
	return rainIntensity;
}

const YsVec3 &FsWeather::GetSkyColor(void) const
{
	return skyColor;
}

const YsVec3 &FsWeather::GetFogColor(void) const
{
	return fogColor;
}

void FsWeather::UpdateRain(const double &dt)
{
	if(isRaining == YSTRUE)
	{
		// Update thunder timer with much faster intervals for dynamic storms
		thunderTimer -= dt;
		if(thunderTimer <= 0.0)
		{
			thunderTimer = 2.0 + (rand() % 8); // Thunder every 2-10 seconds
		}

		// Dynamic rain intensity with fast storm patterns
		static double intensityTime = 0.0;
		static double stormPhase = 0.0;
		intensityTime += dt;
		stormPhase += dt * 0.15; // Much faster storm evolution

		// Complex storm intensity modeling with rapid changes
		double baseIntensity = 0.6 + sin(stormPhase) * 0.3;
		double microVariation = sin(intensityTime * 6.0) * 0.2; // Faster micro variations
		double gusts = sin(intensityTime * 2.5) * sin(intensityTime * 3.2) * 0.25; // Stronger, faster gusts

		rainIntensity = baseIntensity + microVariation + gusts;

		// Clamp to realistic range
		rainIntensity = YsBound(rainIntensity, 0.3, 1.0);

		// Intensify during thunder with faster buildup
		if(thunderTimer > 0.0 && thunderTimer < 2.0)
		{
			double thunderIntensity = (2.0 - thunderTimer) / 2.0;
			rainIntensity += thunderIntensity * 0.6; // Stronger thunder effect
			if(rainIntensity > 1.0) rainIntensity = 1.0;
		}

		// Dynamically adjust sky darkness based on intensity and thunder
		double baseDarkness = 0.12;
		double intensityDarkness = rainIntensity * 0.1;
		double thunderDarkness = 0.0;

		// Make sky darker during thunder buildup with faster cycles
		if(thunderTimer > 0.0 && thunderTimer < 3.0)
		{
			thunderDarkness = (3.0 - thunderTimer) / 3.0 * 0.1; // Moderate darkening
		}

		double totalDarkness = baseDarkness + intensityDarkness - thunderDarkness;
		skyColor.Set(totalDarkness, totalDarkness + 0.04, totalDarkness + 0.1);

		// Adjust fog color to match atmospheric conditions
		fogColor.Set(totalDarkness + 0.15, totalDarkness + 0.2, totalDarkness + 0.25);
	}
}

// Initialize particle system
void InitializeRainParticleSystem()
{
	if(!particleSystemInitialized)
	{
		rainParticles.resize(MAX_RAIN_PARTICLES);
		wetnessPoints.resize(MAX_WETNESS_POINTS);
		for(int i = 0; i < MAX_RAIN_PARTICLES; i++)
		{
			rainParticles[i].active = false;
		}
		for(int i = 0; i < MAX_WETNESS_POINTS; i++)
		{
			wetnessPoints[i].active = false;
		}
		particleSystemInitialized = true;
		srand((unsigned int)time(NULL));
	}
}

// Optimize particle system by removing distant/old particles
void OptimizeParticleSystem(const YsVec3 &cameraPos, double currentTime)
{
	const double maxRenderDistance = 1200.0; // Increased render distance

	for(int i = 0; i < MAX_RAIN_PARTICLES; i++)
	{
		if(rainParticles[i].active)
		{
			rainParticles[i].distanceFromCamera = (rainParticles[i].position - cameraPos).GetLength();
			if(rainParticles[i].distanceFromCamera > maxRenderDistance)
			{
				rainParticles[i].active = false;
			}
		}
	}

	lastOptimizationTime = currentTime;
}

// Add wetness point when rain hits ground
void AddWetnessPoint(const YsVec3 &position, float intensity)
{
	for(int i = 0; i < MAX_WETNESS_POINTS; i++)
	{
		if(!wetnessPoints[i].active)
		{
			wetnessPoints[i].position = position;
			wetnessPoints[i].intensity = intensity;
			wetnessPoints[i].life = 0.0f;
			wetnessPoints[i].maxLife = 8.0f + (rand() % 40) * 0.1f;
			wetnessPoints[i].active = true;
			break;
		}
	}
}

// Get ground elevation using simulation's field system
double GetGroundElevationAt(const YsVec3 &pos, const class FsSimulation *sim)
{
	if(sim != NULL)
	{
		return sim->GetFieldElevation(pos.x(), pos.z());
	}
	return 0.0;
}

void FsWeather::DrawRain(const YsVec3 &cameraPos, const YsVec3 &cameraDir) const
{
	// Legacy method - calls new method with null sim pointer
	DrawRainWithTerrain(cameraPos, cameraDir, nullptr);
}

void FsWeather::DrawRainWithTerrain(const YsVec3 &cameraPos, const YsVec3 &cameraDir, const class FsSimulation *sim) const
{
	if(isRaining != YSTRUE || rainIntensity <= 0.0)
	{
		return;
	}

	// Initialize particle system if needed
	InitializeRainParticleSystem();

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent rain

	// Update particle system with more responsive spawning
	const double dt = 0.0167; // Assume ~60fps for particle updates
	const int particlesToSpawn = (int)(rainIntensity * 50.0);

	// More frequent optimization but less aggressive culling
	static double systemTime = 0.0;
	systemTime += dt;
	if(systemTime - lastOptimizationTime > 0.25) // Optimize every 0.25 seconds instead of 0.5
	{
		OptimizeParticleSystem(cameraPos, systemTime);
	}

	// Spawn new particles
	for(int spawn = 0; spawn < particlesToSpawn; spawn++)
	{
		for(int i = 0; i < MAX_RAIN_PARTICLES; i++)
		{
			if(!rainParticles[i].active)
			{
				// Spawn particle in area around camera
				double angle = (rand() % 360) * YsPi / 180.0;
				double distance = 50.0 + (rand() % 300);
				double height = 80.0 + (rand() % 120);

				rainParticles[i].position.Set(
					cameraPos.x() + cos(angle) * distance,
					cameraPos.y() + height,
					cameraPos.z() + sin(angle) * distance
				);

				// Rain velocity affected by wind and gravity
				rainParticles[i].velocity.Set(
					wind.x() * 0.3 + (rand() % 20 - 10) * 0.1,
					-200.0 - (rand() % 25), // Much faster falling speed
					wind.z() * 0.3 + (rand() % 20 - 10) * 0.1
				);

				rainParticles[i].life = 0.0f;
				rainParticles[i].maxLife = 5.0f + (rand() % 30) * 0.1f;
				rainParticles[i].hitGround = false;
				rainParticles[i].splashLife = 0.0f;
				rainParticles[i].size = 0.8f + (rand() % 5) * 0.1f;
				rainParticles[i].active = true;
				break;
			}
		}
	}

	// Update and render particles
	glBegin(GL_LINES);

	for(int i = 0; i < MAX_RAIN_PARTICLES; i++)
	{
		if(!rainParticles[i].active) continue;

		RainParticle &p = rainParticles[i];

		// Update particle physics
		p.life += dt;
		p.position += p.velocity * dt;

		// Check ground collision with visual ground mesh (always at Y=0)
		// The visual ground mesh is rendered at Y=0 regardless of terrain elevation
		double groundLevel = 0.0;

		if(!p.hitGround && p.position.y() <= groundLevel + 1.0)
		{
			p.hitGround = true;
			p.splashPos = p.position;
			p.splashPos.SetY(groundLevel + 0.1);
			p.splashLife = 0.0f;
		}

		// Remove particle if too old or if it's been on ground too long
		if(p.life >= p.maxLife || (p.hitGround && p.splashLife > 0.5f))
		{
			p.active = false;
			continue;
		}

		// Render raindrop (both falling and hitting ground)
		float alpha = (1200.0f - p.distanceFromCamera) / 1200.0f;
		alpha *= (float)rainIntensity;
		alpha = YsBound(alpha, 0.0f, 0.95f);

		if(alpha > 0.02f)
		{
			if(!p.hitGround)
			{
				// Falling raindrop
				float colorIntensity = 0.85f + (1.0f - p.distanceFromCamera / 1200.0f) * 0.15f;
				glColor4f(colorIntensity, colorIntensity + 0.05f, 1.0f, alpha);

				// Draw raindrop as a short line with size based on distance
				double lineLength = p.size * 3.0;
				if(p.distanceFromCamera > 600.0) lineLength *= 0.8;

				YsVec3 normalizedVel = p.velocity;
				normalizedVel.Normalize();
				YsVec3 dropEnd = p.position + normalizedVel * lineLength;

				glVertex3d(p.position.x(), p.position.y(), p.position.z());
				glVertex3d(dropEnd.x(), dropEnd.y(), dropEnd.z());
			}
			else if(p.splashLife < 0.15f)
			{
				// Ground impact effect - small upward splash
				float splashAlpha = alpha * (0.15f - p.splashLife) / 0.15f;
				glColor4f(0.7f, 0.8f, 0.95f, splashAlpha);

				// Small vertical line for splash effect
				glVertex3d(p.splashPos.x(), p.splashPos.y(), p.splashPos.z());
				glVertex3d(p.splashPos.x(), p.splashPos.y() + 1.5, p.splashPos.z());
			}
		}
	}

	glEnd();

	// Update splash life for all particles
	for(int i = 0; i < MAX_RAIN_PARTICLES; i++)
	{
		if(rainParticles[i].active && rainParticles[i].hitGround)
		{
			rainParticles[i].splashLife += dt;
		}
	}

	glEnd();

	// Additional ground impact points for better visibility
	glPointSize(1.5f);
	glBegin(GL_POINTS);

	for(int i = 0; i < MAX_RAIN_PARTICLES; i++)
	{
		if(!rainParticles[i].active || !rainParticles[i].hitGround) continue;

		RainParticle &p = rainParticles[i];

		if(p.splashLife < 0.1f && p.distanceFromCamera < 150.0)
		{
			float splashAlpha = (0.1f - p.splashLife) / 0.1f;
			splashAlpha *= (float)rainIntensity * 0.6f;

			glColor4f(0.8f, 0.85f, 0.95f, splashAlpha);
			glVertex3d(p.splashPos.x(), p.splashPos.y() + 0.05, p.splashPos.z());
		}
	}

	glEnd();

	// Wetness effects completely disabled to prevent ground rendering issues
	/*
	for(int i = 0; i < MAX_WETNESS_POINTS; i++)
	{
		if(!wetnessPoints[i].active) continue;

		WetnessPoint &w = wetnessPoints[i];
		w.life += dt;

		if(w.life >= w.maxLife)
		{
			w.active = false;
			continue;
		}

		double distanceFromCamera = (w.position - cameraPos).GetLength();
		if(distanceFromCamera < 50.0)
		{
			float wetnessAlpha = (1.0f - w.life / w.maxLife) * w.intensity * 0.1f;

			glColor4f(0.2f, 0.25f, 0.3f, wetnessAlpha);
			glBegin(GL_POINTS);
			glVertex3d(w.position.x(), w.position.y() + 0.01, w.position.z());
			glEnd();
		}
	}
	*/

	// Add extra rain for top-down visibility
	glPointSize(1.0f);
	glBegin(GL_POINTS);

	// Camera direction check for top-down view
	YsVec3 upVector(0.0, 1.0, 0.0);
	double dotProduct = cameraDir * upVector;

	// If looking down (dot product negative), add more visible rain
	if(dotProduct < -0.3)
	{
		float topDownIntensity = (-dotProduct - 0.3f) / 0.7f; // Scale from 0 to 1

		for(int i = 0; i < (int)(topDownIntensity * 200); i++)
		{
			double angle = (rand() % 360) * YsPi / 180.0;
			double radius = (rand() % 400);
			double height = cameraPos.y() - 10.0 - (rand() % 50);

			double x = cameraPos.x() + cos(angle) * radius;
			double z = cameraPos.z() + sin(angle) * radius;

			float alpha = (400.0f - radius) / 400.0f * (float)rainIntensity * topDownIntensity;
			glColor4f(0.8f, 0.85f, 0.95f, alpha);
			glVertex3d(x, height, z);
		}
	}

	glEnd();

	// Enhanced thunder flash effect with faster, more dramatic flashes
	if(thunderTimer > 0.0 && thunderTimer < 1.0)
	{
		float flashIntensity = 0.0f;
		if(thunderTimer < 0.1) // Bright initial flash
		{
			flashIntensity = (0.1f - (float)thunderTimer) / 0.1f;
		}
		else if(thunderTimer < 0.3) // Multiple rapid flickers
		{
			flashIntensity = sin((float)thunderTimer * 80.0f) * 0.6f; // Faster, stronger flickers
			if(flashIntensity < 0) flashIntensity = 0;
		}
		else if(thunderTimer < 0.6) // Additional flickering
		{
			flashIntensity = sin((float)thunderTimer * 120.0f) * 0.3f;
			if(flashIntensity < 0) flashIntensity = 0;
		}

		if(flashIntensity > 0.0f)
		{
			glColor4f(0.9f, 0.95f, 1.0f, flashIntensity * 0.8f); // More intense flashes

			// Large atmospheric flash
			const double flashSize = 8000.0;
			glBegin(GL_QUADS);
			glVertex3d(cameraPos.x() - flashSize, cameraPos.y() + 1200, cameraPos.z() - flashSize);
			glVertex3d(cameraPos.x() + flashSize, cameraPos.y() + 1200, cameraPos.z() - flashSize);
			glVertex3d(cameraPos.x() + flashSize, cameraPos.y() + 1200, cameraPos.z() + flashSize);
			glVertex3d(cameraPos.x() - flashSize, cameraPos.y() + 1200, cameraPos.z() + flashSize);
			glEnd();

			// Directional lightning effect
			glColor4f(1.0f, 1.0f, 1.0f, flashIntensity * 0.8f);
			glBegin(GL_LINES);

			YsVec3 lightningStart = cameraPos + YsVec3((rand() % 2000 - 1000), 400 + (rand() % 200), (rand() % 2000 - 1000));
			YsVec3 lightningEnd = lightningStart + YsVec3((rand() % 200 - 100), -300 - (rand() % 100), (rand() % 200 - 100));

			glVertex3d(lightningStart.x(), lightningStart.y(), lightningStart.z());
			glVertex3d(lightningEnd.x(), lightningEnd.y(), lightningEnd.z());
			glEnd();
		}
	}

	glDepthMask(GL_TRUE);
	glPopAttrib();
}

void FsWeather::DrawCloudLayer(const YsVec3 &cameraPos) const
{
	// Simplified cloud layer - just subtle overhead darkening
	if(weatherType != FSWEATHER_RAIN || cloudLayer.GetN() == 0)
	{
		return;
	}

	glPushAttrib(GL_ALL_ATTRIB_BITS);

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	// Very subtle overhead darkening effect only
	for(int i = 0; i < (int)cloudLayer.GetN(); i++)
	{
		const FsWeatherCloudLayer &layer = cloudLayer[i];

		if(layer.cloudLayerType == FSCLOUDLAYER_OVERCAST && layer.y0 > cameraPos.y() + 100.0)
		{
			// Very subtle, high altitude darkening
			float alpha = (float)rainIntensity * 0.05f;
			glColor4f(0.4f, 0.45f, 0.5f, alpha);

			// Simple overhead rectangle
			double size = 5000.0;
			double height = layer.y0 + 200.0;

			glBegin(GL_QUADS);
			glVertex3d(cameraPos.x() - size, height, cameraPos.z() - size);
			glVertex3d(cameraPos.x() + size, height, cameraPos.z() - size);
			glVertex3d(cameraPos.x() + size, height, cameraPos.z() + size);
			glVertex3d(cameraPos.x() - size, height, cameraPos.z() + size);
			glEnd();
		}
	}

	glPopAttrib();
}
