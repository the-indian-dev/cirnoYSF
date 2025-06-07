#include <ysclass.h>
#include <ysunitconv.h>
#include <ysport.h>
#include <fsgui.h>
#include <assert.h>

#include <ysglparticlemanager.h>

#define FSSIMPLEWINDOW_DONT_INCLUDE_OPENGL_HEADERS
#include <fssimplewindow.h>

#include <fsairproperty.h>

#include "fsconfig.h"

#include "fs.h"
#include "fsradar.h"
#include "fsfilename.h"
#include "fsinstpanel.h"
#include "platform/common/fswindow.h"
#include "graphics/common/fsopengl.h"
#include "graphics/common/fsculling.h"

#include "fspluginmgr.h"
#include "graphics/common/fsfontrenderer.h"
#include "fsguiinfltdlg.h"

#include "fstextresource.h"
#include "ysbitmap.h"

extern FsVisualSrf *cockpit;

void FsSimulation::SimDrawAirplaneSafe(const ActualViewMode &actualViewMode,const FsProjection &proj,unsigned int drawFlag) const
{
	auto &viewPoint=actualViewMode.viewPoint;
	auto &viewMat=actualViewMode.viewMat;

	// Optimized airplane rendering with comprehensive safety checks
	static int drawCount = 0;
	int visibleCount = 0;
	
	FsAirplane *seeker;
	seeker=NULL;
	while((seeker=FindNextAirplane(seeker))!=NULL)
	{
		// Critical safety check: verify object is still valid and alive
		if(seeker==NULL)
		{
			printf("DEBUG: Null aircraft pointer detected, skipping\n");
			continue;
		}
		
		// Check if object is still alive (might have been destroyed by missile)
		YSBOOL isAlive = YSFALSE;
		try {
			isAlive = seeker->IsAlive();
		} catch (...) {
			// Object might be in invalid state during destruction
			printf("DEBUG: Aircraft in invalid state during destruction, skipping\n");
			continue;
		}
		
		if(isAlive != YSTRUE)
		{
			printf("DEBUG: Dead aircraft detected, skipping\n");
			continue;
		}
		
		if(actualViewMode.actualViewMode==FSBOMBINGVIEW &&
		   IsPlayerAirplane(seeker)==YSTRUE)
		{
			continue;
		}

		// Safe distance calculation with error handling
		YsVec3 airPos;
		double airRad;
		double distance;
		
		try {
			airPos = seeker->GetPosition();
			airRad = seeker->GetApproximatedCollideRadius();
			distance = (airPos-viewPoint).GetLength();
			
			// Sanity check on values
			if(airRad <= 0.0 || distance < 0.0 || distance > 1000000.0)
			{
				continue;
			}
		} catch (...) {
			// If any position/radius access fails, skip this object
			printf("DEBUG: Aircraft position/radius access failed, skipping\n");
			continue;
		}
		
		// Enhanced LOD system with more aggressive culling
		YSBOOL drawCoarseOrdinance = cfgPtr->drawCoarseOrdinance;
		int lodLevel = 0;
		
		switch(cfgPtr->airLod)
		{
		case 0: // Automatic with enhanced distance checks
			{
				double lodDist = airRad * 16.0 * actualViewMode.viewMagFix * viewMagUser;
				if(distance > lodDist * 2.0)
				{
					lodLevel = 2; // Super coarse for very distant objects
					drawCoarseOrdinance = YSTRUE;
				}
				else if(distance > lodDist)
				{
					lodLevel = 1; // Coarse
					drawCoarseOrdinance = YSTRUE;
				}
				else
				{
					lodLevel = 0; // Full detail
				}
			}
			break;
		case 1: // Always High Quality
			lodLevel = 0;
			break;
		case 2: // Always Coarse
			lodLevel = 1;
			break;
		case 3: // Super-coarse
			lodLevel = 2;
			break;
		}

		// Critical safety check before drawing - object might be destroyed during frame
		try {
			if(seeker!=NULL && seeker->IsAlive()==YSTRUE)
			{
				// Draw airplane with selected LOD
				if(lodLevel >= 2)
				{
					seeker->UntransformedCollisionShell().Draw(
					    actualViewMode.viewMat,proj.GetMatrix(),seeker->GetPosition(),seeker->GetAttitude(),drawFlag);
				}
				else
				{
					seeker->Draw(lodLevel,actualViewMode.viewMat,proj.GetMatrix(),viewPoint,drawFlag,currentTime);
				}
				
				visibleCount++;

				// Handle ordinance rendering with extra safety for missile impacts
				if(cfgPtr->drawOrdinance==YSTRUE && distance < 5000.0)
				{
					// Double-check object is still alive before drawing ordinance
					if(seeker->IsAlive()==YSTRUE)
					{
						seeker->Prop().DrawOrdinanceVisual(
						    drawCoarseOrdinance,seeker->weaponShapeOverrideStatic,actualViewMode.viewMat,proj.GetMatrix(),drawFlag);
					}
				}
			}
		} catch (...) {
			// If drawing fails (object destroyed during draw), skip silently
			printf("DEBUG: Aircraft drawing failed (object destroyed during draw)\n");
			continue;
		}

		// Handle cockpit rendering for player aircraft with safety checks
		if((actualViewMode.actualViewMode==FSCOCKPITVIEW ||
		    actualViewMode.actualViewMode==FSADDITIONALAIRPLANEVIEW ||
		    actualViewMode.actualViewMode==FSADDITIONALAIRPLANEVIEW_CABIN) &&
		   IsPlayerAirplane(seeker)==YSTRUE)
		{
			try {
				if(seeker!=NULL && seeker->IsAlive()==YSTRUE && seeker->cockpit!=nullptr)
				{
					seeker->cockpit.Draw(actualViewMode.viewMat,proj.GetMatrix(),seeker->GetPosition(),seeker->GetAttitude(),drawFlag);
				}
				else if(seeker!=NULL && seeker->IsAlive()==YSTRUE && cockpit!=nullptr)
				{
					cockpit->Draw(actualViewMode.viewMat,proj.GetMatrix(),seeker->GetPosition(),seeker->GetAttitude(),drawFlag);
				}
			} catch (...) {
				// If cockpit drawing fails, continue without it
			}
		}
	}

	// Performance statistics (debug)
	drawCount++;
	if(drawCount % 300 == 0) // Print stats every 300 frames
	{
		printf("Aircraft drawn safely: %d/%d (frame %d)\n", visibleCount, GetNumAirplane(), drawCount);
	}
}

void FsSimulation::SimDrawGroundSafe(const ActualViewMode &actualViewMode,const FsProjection &proj,unsigned int drawFlag) const
{
	auto &viewPoint=actualViewMode.viewPoint;
	auto &viewMat=actualViewMode.viewMat;

	// Optimized ground vehicle rendering with comprehensive safety checks
	static int groundDrawCount = 0;
	int visibleGroundCount = 0;

	FsGround *seeker;
	seeker=NULL;
	while((seeker=FindNextGround(seeker))!=NULL)
	{
		// Critical safety check: verify object is still valid and alive
		if(seeker==NULL)
		{
			printf("DEBUG: Null ground vehicle pointer detected, skipping\n");
			continue;
		}
		
		// Check if object is still alive (might have been destroyed by missile)
		YSBOOL isAlive = YSFALSE;
		try {
			isAlive = seeker->IsAlive();
		} catch (...) {
			// Object might be in invalid state during destruction
			printf("DEBUG: Ground vehicle in invalid state during destruction, skipping\n");
			continue;
		}
		
		if(isAlive != YSTRUE)
		{
			printf("DEBUG: Dead ground vehicle detected, skipping\n");
			continue;
		}
		
		if(actualViewMode.actualViewMode==FSBOMBINGVIEW &&
		   GetPlayerGround()==seeker)
		{
			continue;
		}

		// Safe distance and size calculation with error handling
		double objRad;
		double distance;
		double apparentRad;
		
		try {
			objRad = seeker->Prop().GetOutsideRadius();
			distance = (seeker->GetPosition()-viewPoint).GetLength();
			apparentRad = objRad * proj.prjPlnDist / distance;
			
			// Sanity check on values
			if(objRad <= 0.0 || distance < 0.0 || distance > 1000000.0)
			{
				continue;
			}
		} catch (...) {
			// If any access fails, skip this object
			printf("DEBUG: Ground vehicle position/radius access failed, skipping\n");
			continue;
		}

		// More aggressive culling - only draw if reasonably visible
		if(apparentRad >= 0.5)  // Reduced threshold for better performance
		{
			// Enhanced LOD system for ground vehicles
			int lodLevel = 0;
			switch(cfgPtr->gndLod)
			{
			case 0: // Automatic with distance-based decisions
				if(distance > objRad * 50.0)
				{
					lodLevel = 2; // Super coarse
				}
				else if(distance > objRad * 20.0)
				{
					lodLevel = 1; // Coarse
				}
				else
				{
					lodLevel = 0; // Full detail
				}
				break;
			case 1: // Always High Quality
				lodLevel = 0;
				break;
			case 2: // Always Coarse
				lodLevel = 1;
				break;
			case 3: // Super-coarse
				lodLevel = 2;
				break;
			}

			// Critical safety check before drawing - object might be destroyed during frame
			try {
				if(seeker!=NULL && seeker->IsAlive()==YSTRUE)
				{
					// Draw with selected LOD
					if(lodLevel >= 2)
					{
						seeker->UntransformedCollisionShell().Draw(
						    viewMat,proj.GetMatrix(),seeker->GetPosition(),seeker->GetAttitude(),drawFlag);
					}
					else
					{
						seeker->Draw(lodLevel,viewMat,proj.GetMatrix(),viewPoint,drawFlag,currentTime);
					}
					
					visibleGroundCount++;
				}
			} catch (...) {
				// If drawing fails (object destroyed during draw), skip silently
				printf("DEBUG: Ground vehicle drawing failed (object destroyed during draw)\n");
				continue;
			}
		}

		// Handle cockpit rendering for player ground vehicle with safety checks
		if((actualViewMode.actualViewMode==FSCOCKPITVIEW ||
		    actualViewMode.actualViewMode==FSADDITIONALAIRPLANEVIEW ||
		    actualViewMode.actualViewMode==FSADDITIONALAIRPLANEVIEW_CABIN) &&
		   GetPlayerGround()==seeker)
		{
			try {
				if(seeker!=NULL && seeker->IsAlive()==YSTRUE && seeker->cockpit!=nullptr)
				{
					seeker->cockpit.Draw(actualViewMode.viewMat,proj.GetMatrix(),seeker->GetPosition(),seeker->GetAttitude(),drawFlag);
				}
				else if(cockpit!=nullptr)
				{
					cockpit->Draw(actualViewMode.viewMat,proj.GetMatrix(),seeker->GetPosition(),seeker->GetAttitude(),drawFlag);
				}
			} catch (...) {
				// If cockpit drawing fails, continue without it
			}
		}
	}

	// Handle aircraft carriers with comprehensive safety checks
	FsAircraftCarrierProperty::BeginDrawArrestingWire();
	for(int i=0; i<aircraftCarrierList.GetN(); i++)
	{
		if(aircraftCarrierList[i]!=NULL)
		{
			try {
				// Check if carrier is still alive
				if(aircraftCarrierList[i]->IsAlive()!=YSTRUE)
				{
					continue;
				}
				
				// Distance check for aircraft carriers
				double carrierDistance = (aircraftCarrierList[i]->GetPosition()-viewPoint).GetLength();
				if(carrierDistance < 10000.0) // Only draw carriers within 10km
				{
					// Use appropriate LOD for aircraft carriers
					int carrierLOD = (carrierDistance > 5000.0) ? 1 : 0;
					aircraftCarrierList[i]->Draw(carrierLOD, viewMat, proj.GetMatrix(), viewPoint, drawFlag, currentTime);
					
					// Only draw detailed bridge and wires if close enough and still alive
					if(carrierDistance < 3000.0 && aircraftCarrierList[i]->IsAlive()==YSTRUE)
					{
						auto carrierProp = aircraftCarrierList[i]->Prop().GetAircraftCarrierProperty();
						if(carrierProp != nullptr)
						{
							carrierProp->DrawBridge(viewMat);
							carrierProp->DrawArrestingWire();
						}
					}
				}
			} catch (...) {
				// If carrier drawing fails, skip this carrier
				printf("DEBUG: Aircraft carrier drawing failed\n");
				continue;
			}
		}
	}
	FsAircraftCarrierProperty::EndDrawArrestingWire();

	// Performance statistics (debug)
	groundDrawCount++;
	if(groundDrawCount % 300 == 0) // Print stats every 300 frames
	{
		printf("Ground vehicles drawn safely: %d (frame %d)\n", visibleGroundCount, groundDrawCount);
	}
}