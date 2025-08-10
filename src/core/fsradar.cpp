#include "fsradar.h"
#include "graphics/common/fsopengl.h"



////////////////////////////////////////////////////////////

// Ground -> CROSS
// Air -> RECT
// Friendly -> White
// Hostile -> Green

void FsHorizontalRadar::Draw(
	 const FsSimulation *sim,int x1,int y1,int x2,int y2,const double &rangeInX,const FsAirplane &withRespectTo,
	 int mode,const double &airAltLimit) const
	// mode 0:air to air    1: air to gnd    2:bombing
{
	const double mag=double(YsAbs(x2-x1))/(rangeInX*1600.0); // Conversion from meters to miles
	const int mkSize=3;


	YsVec2 w1,w2,wc;
	w1.Set(x1,y1);
	w2.Set(x2,y2);
	if(mode==0)
	{
		wc=(w1+w2)/2.0;
	}
	else
	{
		wc.Set((w1.x()+w2.x())/2.0,(w1.y()+w2.y()*3.0)/4.0);
	}


	YsAtt3 attH;
	attH=withRespectTo.GetAttitude();
	attH.SetP(0.0);
	attH.SetB(0.0);


	YsMatrix4x4 ref;
	ref.Translate(withRespectTo.GetPosition());
	ref.Rotate(attH);

	DrawBasic(sim,x1,y1,x2,y2,rangeInX,withRespectTo,withRespectTo.GetPosition(),withRespectTo.GetAttitude(),mode,airAltLimit);

	switch(mode)
	{
	case 0:
	case 1:
		{
			double radar,dy,dx;
			YsVec2 wr;
			if(mode==0)
			{
				radar=withRespectTo.Prop().GetAAMRadarAngle();
			}
			else
			{
				radar=withRespectTo.Prop().GetAGMRadarAngle();
			}
			dy=double(YsAbs(y2-y1))/2.0;
			dx=dy*sin(radar);
			wr.Set(wc.x()-dx,w1.y());
			const int x =(int)wc.x();
			const int y =(int)wc.y();
			int xx=(int)wr.x();
			int yy=(int)wr.y();
			FsDrawLine(x,y,xx,yy,YsGreen());
			wr.Set(wc.x()+dx,w1.y());
			xx=(int)wr.x();
			yy=(int)wr.y();
			FsDrawLine(x,y,xx,yy,YsGreen());
		}
		break;
	case 2:
		{
			YsVec3 bomb;
			if(withRespectTo.Prop().ComputeEstimatedBombLandingPosition(bomb,sim->GetWeather())==YSOK)
			{
				YsVec2 prj;

				ref.MulInverse(bomb,bomb,1.0);
				bomb*=mag;
				prj.Set(bomb.x(),-bomb.z());
				prj+=wc;

				const int x=(int)prj.x();
				const int y=(int)prj.y();
				if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
				{
					FsDrawDiamond(x,y,mkSize,YsGreen(),YSFALSE);
				}

				FsDrawLine((int)wc.x()-3,y1,(int)wc.x()-3,y2,YsGreen());
				FsDrawLine((int)wc.x()+3,y1,(int)wc.x()+3,y2,YsGreen());
			}
		}
		break;
	}
}

void FsHorizontalRadar::DrawBasic(
    const class FsSimulation *sim,int x1,int y1,int x2,int y2,const double &rangeInX,
    const FsExistence &withRespectTo,const YsVec3 &pos,const YsAtt3 &att,
    int mode,const double &airAltLimit) const
{
	const double mag=double(YsAbs(x2-x1))/(rangeInX*1600.0); // Conversion from meters to miles
	int x,y,xx,yy;
	YsAtt3 attH;
	YsMatrix4x4 ref;
	YsVec2 w1,w2,wc;
	YsColor col;
	const int mkSize=3;

	//FsDrawRect(x1,y1,x2,y2,YsGreen(),YSFALSE);

	//moved to bottom for rendering order
	//char str[256];
	//sprintf(str,"%d MILES",int(rangeInX));
	//FsDrawString(x1+8,y1+24,str,YsGreen());


	attH=att;
	attH.SetP(0.0);
	attH.SetB(0.0);

	ref.Translate(pos);
	ref.Rotate(attH);

	w1.Set(x1,y1);
	w2.Set(x2,y2);
	if(mode==0)
	{
		wc=(w1+w2)/2.0;
	}
	else
	{
		wc.Set((w1.x()+w2.x())/2.0,(w1.y()+w2.y()*3.0)/4.0);
	}

	//x=(int)wc.x();
	//y=(int)wc.y();
	//FsDrawLine(x-5,y  ,x+5,y  ,YsGreen());
	//FsDrawPoint(x+5,y,YsGreen());
	//FsDrawLine(x  ,y-5,x  ,y+5,YsGreen());
	//FsDrawPoint(x,y+5,YsGreen());

	// First pass: Draw SAM ranges in background
	const FsGround *gnd;
	gnd=NULL;
	while((gnd=sim->FindNextGround(gnd))!=NULL)
	{
		if(gnd->IsAlive()==YSTRUE && gnd->Prop().IsNonGameObject()!=YSTRUE)
		{
			YsVec3 pos;
			YsVec2 prj;

			ref.MulInverse(pos,gnd->GetPosition(),1.0);

			pos*=mag;

			prj.Set(pos.x(),-pos.z());
			prj+=wc;

			if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
			{
				// Only draw SAM ranges for enemy units
				if(withRespectTo.iff!=gnd->iff && gnd->Prop().GetNumSAM() > 0)
				{
					double samRange = gnd->Prop().GetSAMRange();
					int pixelRadius = int(samRange * mag);

					YsColor redAlpha;
					redAlpha.SetIntRGBA(255, 0, 0, 50);

					// Use clipped version to stay within radar bounds
					FsDrawCircleTransparentClipped((int)prj.x(), (int)prj.y(), pixelRadius, redAlpha, YSTRUE, x1, y1, x2, y2);
				}
			}
		}
	}

	// Second pass: Draw ground units
	//Draw after the red circles are done
	FsDrawRect(x1,y1,x2,y2,YsGreen(),YSFALSE);

	char str[256];
	sprintf(str,"%d MILES",int(rangeInX));
	FsDrawString(x1+8,y1+24,str,YsGreen());

	x=(int)wc.x();
	y=(int)wc.y();
	FsDrawLine(x-5,y  ,x+5,y  ,YsGreen());
	FsDrawPoint(x+5,y,YsGreen());
	FsDrawLine(x  ,y-5,x  ,y+5,YsGreen());
	FsDrawPoint(x,y+5,YsGreen());



	gnd=NULL;
	while((gnd=sim->FindNextGround(gnd))!=NULL)
	{
		if(gnd->IsAlive()==YSTRUE && gnd->Prop().IsNonGameObject()!=YSTRUE)
		{
			YsVec3 pos;
			YsVec2 prj;

			ref.MulInverse(pos,gnd->GetPosition(),1.0);

			pos*=mag;

			prj.Set(pos.x(),-pos.z());
			prj+=wc;

			if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
			{
				if(withRespectTo.iff==gnd->iff)
				{
					col=YsWhite();
				}
				else
				{
					col=YsGreen();
				}

				FsDrawX((int)prj.x(),(int)prj.y(),mkSize,col);
			}
		}
	}

	const FsAirplane *air;
	air=NULL;
	while((air=sim->FindNextAirplane(air))!=NULL)
	{
		if(air!=&withRespectTo && air->IsAlive()==YSTRUE)
		{
			double altLimit;
			altLimit=airAltLimit+1000.0*(1.0-air->Prop().GetRadarCrossSection());
			if(altLimit<air->GetPosition().y())
			{

				YsVec3 pos,vel;
				YsVec2 prj1,prj2;

				air->Prop().GetVelocity(vel);

				ref.MulInverse(pos,air->GetPosition(),1.0);
				ref.MulInverse(vel,vel,0.0);

				if(vel.Normalize()==YSOK)
				{
					vel*=8.0;
				}

				pos*=mag;
				vel+=pos;

				prj1.Set(pos.x(),-pos.z());
				prj2.Set(vel.x(),-vel.z());

				prj1+=wc;
				prj2+=wc;

				if(YsCheckInsideBoundingBox2(prj1,w1,w2)==YSTRUE)
				{
					if(air->Prop().IsActive()!=YSTRUE)
					{
						col=YsBlack();
					}
					else if(withRespectTo.iff==air->iff)
					{
						col=YsWhite();
					}
					else
					{
						col=YsGreen();
					}

					x=(int)prj1.x();
					y=(int)prj1.y();
					FsDrawRect(x-mkSize+1,y-mkSize+1,x+mkSize-1,y+mkSize-1,col,YSTRUE);

					if(YsCheckInsideBoundingBox2(prj2,w1,w2)==YSTRUE)
					{
						xx=(int)prj2.x();
						yy=(int)prj2.y();
						FsDrawLine(x,y,xx,yy,col);
					}
				}
			}
		}
	}

	const FsWeapon *wpn;
	wpn=NULL;
	while((wpn=sim->FindNextActiveWeapon(wpn))!=NULL)
	{
		if(wpn->lifeRemain>YsTolerance && wpn->timeRemain>YsTolerance)
		{
			if(wpn->type==FSWEAPON_AIM9 || wpn->type==FSWEAPON_AIM120)
			{
				col=YsRed();
			}
			else if(wpn->type==FSWEAPON_AIM9X)
			{
				col=YsBlue();
			}
			else if(wpn->type==FSWEAPON_AGM65 || wpn->type==FSWEAPON_ROCKET)
			{
				col=YsYellow();
			}
			else
			{
				continue;
			}

			YsVec3 pos;
			YsVec2 prj;

			ref.MulInverse(pos,wpn->pos,1.0);

			pos*=mag;

			prj.Set(pos.x(),-pos.z());

			prj+=wc;

			if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
			{
				FsDrawPoint2Pix((int)prj.x(),(int)prj.y(),col);
			}
		}
	}
}

////////////////////////////////////////////////////////////

// Vertical Radar (B-Scope) Implementation
// Y-axis: Range to target
// X-axis: Horizontal angle relative to aircraft heading

void FsVerticalRadar::Draw(
	 const FsSimulation *sim,int x1,int y1,int x2,int y2,const double &rangeInX,const FsAirplane &withRespectTo,
	 int mode,const double &airAltLimit) const
	// mode 0:air to air    1: air to gnd    2:bombing
{
	const double mag=double(YsAbs(y2-y1))/(rangeInX*1600.0); // Conversion from meters to miles
	const int mkSize=3;

	YsVec2 w1,w2,wc;
	w1.Set(x1,y1);
	w2.Set(x2,y2);
	wc=(w1+w2)/2.0;

	YsAtt3 attH;
	attH=withRespectTo.GetAttitude();
	attH.SetP(0.0);
	attH.SetB(0.0);

	YsMatrix4x4 ref;
	ref.Translate(withRespectTo.GetPosition());
	ref.Rotate(attH);

	DrawBasic(sim,x1,y1,x2,y2,rangeInX,withRespectTo,withRespectTo.GetPosition(),withRespectTo.GetAttitude(),mode,airAltLimit);

	switch(mode)
	{
	case 0:
	case 1:
		{
			double radar;
			if(mode==0)
			{
				radar=withRespectTo.Prop().GetAAMRadarAngle();
			}
			else
			{
				radar=withRespectTo.Prop().GetAGMRadarAngle();
			}

			// Draw radar scan cone for B-scope display
			double angleRange = YsPi/2.0; // 90 degrees total view (±45°)
			double maxRange = rangeInX * 1600.0; // Convert to meters

			// Calculate cone edges at different ranges
			double leftAngle = -radar;
			double rightAngle = radar;

			// Draw cone at multiple range intervals
			for(int i = 1; i <= 4; ++i)
			{
				double coneRange = rangeInX * i / 4.0 * 1600.0; // Range in meters
				int coneY = y2 - (int)(coneRange * mag);

				if(coneY > y1)
				{
					int leftX = wc.x() + leftAngle * double(YsAbs(x2-x1))/(2.0*angleRange);
					int rightX = wc.x() + rightAngle * double(YsAbs(x2-x1))/(2.0*angleRange);

					// Ensure cone edges stay within display bounds
					leftX = YsGreater(leftX, x1);
					rightX = YsSmaller(rightX, x2);

					// Draw cone arc at this range
					FsDrawLine(leftX, coneY, rightX, coneY, YsGreen());
				}
			}

			// Draw cone edge lines from center to maximum range
			double maxConeRange = rangeInX * 1600.0;
			int maxConeY = y2 - (int)(maxConeRange * mag);
			if(maxConeY < y1) maxConeY = y1;

			int leftX = wc.x() + leftAngle * double(YsAbs(x2-x1))/(2.0*angleRange);
			int rightX = wc.x() + rightAngle * double(YsAbs(x2-x1))/(2.0*angleRange);

			// Ensure cone edges stay within display bounds
			leftX = YsGreater(leftX, x1);
			rightX = YsSmaller(rightX, x2);

			// Draw cone boundary lines
			FsDrawLine((int)wc.x(), y2, leftX, maxConeY, YsGreen());
			FsDrawLine((int)wc.x(), y2, rightX, maxConeY, YsGreen());
		}
		break;
	case 2:
		{
			YsVec3 bomb;
			if(withRespectTo.Prop().ComputeEstimatedBombLandingPosition(bomb,sim->GetWeather())==YSOK)
			{
				YsVec2 prj;
				YsVec3 relPos;

				ref.MulInverse(relPos,bomb,1.0);

				// Calculate range and bearing like horizontal radar
				double horizontalRange = sqrt(relPos.x()*relPos.x() + relPos.z()*relPos.z());
				double angle = atan2(relPos.x(), relPos.z()); // Bearing relative to aircraft heading

				// Only show if target is in front hemisphere
				if(relPos.z() > 0.0) // Target is in front (negative z is forward)
				{
					// Convert to screen coordinates
					double angleRange = YsPi/2.0; // 90 degrees total view (±45°)
					prj.Set(wc.x() + angle * double(YsAbs(x2-x1))/(2.0*angleRange),
					        y2 - horizontalRange * mag);
				}

				const int x=(int)prj.x();
				const int y=(int)prj.y();
				if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
				{
					FsDrawDiamond(x,y,mkSize,YsGreen(),YSFALSE);
				}
			}
		}
		break;
	}
}

	void FsVerticalRadar::DrawBasic(
	    const class FsSimulation *sim,int x1,int y1,int x2,int y2,const double &rangeInX,
	    const FsExistence &withRespectTo,const YsVec3 &pos,const YsAtt3 &att,
	    int mode,const double &airAltLimit) const
	{
		const double mag=double(YsAbs(y2-y1))/(rangeInX*1600.0); // Conversion from meters to miles
		// print for debug, y2, y2, rangeInX, mag
		// printf("y1: %d, y2: %d, rangeInX: %.2f, mag: %.2f\n", y1, y2, rangeInX, mag);
		int x,y,xx,yy;
		YsAtt3 attH;
		YsMatrix4x4 ref;
		YsVec2 w1,w2,wc;
		YsColor col;
		const int mkSize=3;

		// Use only heading for reference (no pitch or bank)
		attH=att;
		attH.SetP(0.0);
		attH.SetB(0.0);

		ref.Translate(pos);
		ref.Rotate(attH);

		w1.Set(x1,y1);
		w2.Set(x2,y2);
		wc.Set((w1.x()+w2.x())/2.0, y2); // Center horizontally, bottom vertically

		// Draw radar frame
		FsDrawRect(x1,y1,x2,y2,YsGreen(),YSFALSE);

		char str[256];
		sprintf(str,"B-SCOPE %d MILES",int(rangeInX));
		FsDrawString(x1+8,y2-16,str,YsGreen());

		// Draw center vertical line (aircraft heading)
		x=(int)wc.x();
		FsDrawLine(x,y1,x,y2,YsGreen());

		// Draw range rings with labels
		for(int i=1; i<=4; ++i)
		{
			double ringRange = rangeInX * i / 4.0;
			int ringY = y2 - (int)(ringRange * 1600.0 * mag);
			if(ringY > y1)
			{
				FsDrawLine(x1,ringY,x2,ringY,YsGreen());
				// Add range labels
				sprintf(str,"%.0f",ringRange);
				FsDrawString(x1+2,ringY-2,str,YsGreen());
			}
		}

		// Draw angle markers and labels
		double angleRange = YsPi/2.0; // 90 degrees total view (±45°)
		for(int i = -3; i <= 3; ++i)
		{
			if(i != 0)
			{
				double angle = i * YsPi/12.0; // 15 degree increments
				int markerX = wc.x() + angle * double(YsAbs(x2-x1))/(2.0*angleRange);
				if(markerX > x1 && markerX < x2)
				{
					FsDrawLine(markerX,y1,markerX,y1+15,YsGreen());
					// Add angle labels
					sprintf(str,"%d°",i*15);
					FsDrawString(markerX-10,y1+30,str,YsGreen());
				}
			}
		}

		// Draw scale labels
		FsDrawString(x1+8,y1+35,"RANGE",YsGreen());
		FsDrawString(wc.x()-20,y1+50,"ANGLE",YsGreen());

	// First pass: Draw SAM ranges in background
	const FsGround *gnd;
	gnd=NULL;
	while((gnd=sim->FindNextGround(gnd))!=NULL)
	{
		if(gnd->IsAlive()==YSTRUE && gnd->Prop().IsNonGameObject()!=YSTRUE)
		{
			YsVec3 relPos;
			YsVec2 prj;

			ref.MulInverse(relPos,gnd->GetPosition(),1.0);

			// Calculate range and bearing like horizontal radar
			double horizontalRange = sqrt(relPos.x()*relPos.x() + relPos.z()*relPos.z());
			double angle = atan2(relPos.x(), relPos.z()); // Bearing relative to aircraft heading

			// Only show targets in front hemisphere (within ±90 degrees)
			if(relPos.z() <= 0.0) // Target is behind aircraft (negative z is forward)
			{
				continue;
			}

			// Convert to screen coordinates
			prj.Set(wc.x() + angle * double(YsAbs(x2-x1))/(2.0*angleRange),
			        y2 - horizontalRange * mag);

			if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
			{
				// Only draw SAM ranges for enemy units
				if(withRespectTo.iff!=gnd->iff && gnd->Prop().GetNumSAM() > 0)
				{
					double samRange = gnd->Prop().GetSAMRange();
					int pixelRadius = int(samRange * mag);

					YsColor redAlpha;
					redAlpha.SetIntRGBA(255, 0, 0, 50);

					// Draw vertical threat zone
					int threatTop = YsGreater(y1, (int)prj.y() - pixelRadius);
					int threatBottom = YsSmaller(y2, (int)prj.y() + pixelRadius);
					if(threatTop < threatBottom)
					{
						FsDrawRect((int)prj.x()-3, threatTop, (int)prj.x()+3, threatBottom, redAlpha, YSTRUE);
					}
				}
			}
		}
	}

	// Second pass: Draw ground units
	gnd=NULL;
	while((gnd=sim->FindNextGround(gnd))!=NULL)
	{
		if(gnd->IsAlive()==YSTRUE && gnd->Prop().IsNonGameObject()!=YSTRUE)
		{
			YsVec3 relPos;
			YsVec2 prj;

			ref.MulInverse(relPos,gnd->GetPosition(),1.0);

			// Calculate range and bearing like horizontal radar
			double horizontalRange = sqrt(relPos.x()*relPos.x() + relPos.z()*relPos.z());
			double angle = atan2(relPos.x(), relPos.z()); // Bearing relative to aircraft heading

			// Only show targets in front hemisphere (within ±90 degrees)
			if(relPos.z() <= 0.0) // Target is behind aircraft (negative z is forward)
			{
				continue;
			}

			// Convert to screen coordinates
			prj.Set(wc.x() + angle * double(YsAbs(x2-x1))/(2.0*angleRange),
			        y2 - horizontalRange * mag);

			if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
			{
				if(withRespectTo.iff==gnd->iff)
				{
					col=YsWhite();
				}
				else
				{
					col=YsGreen();
				}

				FsDrawX((int)prj.x(),(int)prj.y(),mkSize,col);
			}
		}
	}

	// Draw airplanes
	const FsAirplane *air;
	air=NULL;
	while((air=sim->FindNextAirplane(air))!=NULL)
	{
		if(air!=&withRespectTo && air->IsAlive()==YSTRUE)
		{
			double altLimit;
			altLimit=airAltLimit+1000.0*(1.0-air->Prop().GetRadarCrossSection());
			if(altLimit<air->GetPosition().y())
			{
				YsVec3 relPos,vel;
				YsVec2 prj1,prj2;

				air->Prop().GetVelocity(vel);
				ref.MulInverse(relPos,air->GetPosition(),1.0);
				ref.MulInverse(vel,vel,0.0);

				// Calculate range and bearing like horizontal radar
				double horizontalRange = sqrt(relPos.x()*relPos.x() + relPos.z()*relPos.z());
				double angle = atan2(relPos.x(), relPos.z()); // Bearing relative to aircraft heading

				// Only show targets in front hemisphere (within ±90 degrees)
				if(relPos.z() <= 0.0) // Target is behind aircraft (negative z is forward)
				{
					continue;
				}

				// Convert to screen coordinates
				prj1.Set(wc.x() + angle * double(YsAbs(x2-x1))/(2.0*angleRange),
				         y2 - horizontalRange * mag);

				// Calculate velocity vector for display
				if(vel.Normalize()==YSOK)
				{
					vel*=8.0;
					double velAngle = atan2(vel.x(), -vel.z());
					double velRange = horizontalRange + 8.0; // Fixed velocity vector length
					prj2.Set(wc.x() + velAngle * double(YsAbs(x2-x1))/(2.0*angleRange),
					         y2 - velRange * mag);
				}
				else
				{
					prj2 = prj1;
				}

				if(YsCheckInsideBoundingBox2(prj1,w1,w2)==YSTRUE)
				{
					if(air->Prop().IsActive()!=YSTRUE)
					{
						col=YsBlack();
					}
					else if(withRespectTo.iff==air->iff)
					{
						col=YsWhite();
					}
					else
					{
						col=YsGreen();
					}

					x=(int)prj1.x();
					y=(int)prj1.y();
					FsDrawRect(x-mkSize+1,y-mkSize+1,x+mkSize-1,y+mkSize-1,col,YSTRUE);

					// Draw range and bearing info next to target
					if(horizontalRange > 0.1) // Avoid division by zero
					{
						sprintf(str,"%.1f/%.0f°",horizontalRange/1600.0,angle*180.0/YsPi);
						FsDrawString(x+mkSize+2,y,str,col);
					}

					if(YsCheckInsideBoundingBox2(prj2,w1,w2)==YSTRUE)
					{
						xx=(int)prj2.x();
						yy=(int)prj2.y();
						FsDrawLine(x,y,xx,yy,col);
					}
				}
			}
		}
	}

	// Draw weapons
	const FsWeapon *wpn;
	wpn=NULL;
	while((wpn=sim->FindNextActiveWeapon(wpn))!=NULL)
	{
		if(wpn->lifeRemain>YsTolerance && wpn->timeRemain>YsTolerance)
		{
			if(wpn->type==FSWEAPON_AIM9 || wpn->type==FSWEAPON_AIM120)
			{
				col=YsRed();
			}
			else if(wpn->type==FSWEAPON_AIM9X)
			{
				col=YsBlue();
			}
			else if(wpn->type==FSWEAPON_AGM65 || wpn->type==FSWEAPON_ROCKET)
			{
				col=YsYellow();
			}
			else
			{
				continue;
			}

			YsVec3 relPos;
			YsVec2 prj;

			ref.MulInverse(relPos,wpn->pos,1.0);

			// Calculate range and bearing like horizontal radar
			double horizontalRange = sqrt(relPos.x()*relPos.x() + relPos.z()*relPos.z());
			double angle = atan2(relPos.x(), relPos.z()); // Bearing relative to aircraft heading

			// Only show targets in front hemisphere (within ±90 degrees)
			if(-relPos.z() <= 0.0) // Target is behind aircraft (negative z is forward)
			{
				continue;
			}

			// Convert to screen coordinates
			prj.Set(wc.x() + angle * double(YsAbs(x2-x1))/(2.0*angleRange),
			        y2 - horizontalRange * mag);

			if(YsCheckInsideBoundingBox2(prj,w1,w2)==YSTRUE)
			{
				FsDrawPoint2Pix((int)prj.x(),(int)prj.y(),col);
			}
		}
	}
}
