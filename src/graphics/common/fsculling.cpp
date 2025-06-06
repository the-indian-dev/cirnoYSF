#include "fsculling.h"
#include <ysclass.h>
#include <ysdef.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#define FSSIMPLEWINDOW_DONT_INCLUDE_OPENGL_HEADERS
#include <fssimplewindow.h>

#ifndef __APPLE__
#include <GL/gl.h>
#include <GL/glu.h>
#else
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#endif

FsCullingUtil::Frustum FsCullingUtil::CreateFrustumFromCurrentMatrix(void)
{
    // Create a local frustum variable
    Frustum frustum;

    // Get the current modelview and projection matrices
    GLfloat modelMatrix[16];
    GLfloat projMatrix[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelMatrix);
    glGetFloatv(GL_PROJECTION_MATRIX, projMatrix);

    // Combine the matrices (clip = proj * model)
    GLfloat clipMatrix[16];

    // Multiply matrices: clipMatrix = projMatrix * modelMatrix
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            clipMatrix[i*4+j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                clipMatrix[i*4+j] += projMatrix[i*4+k] * modelMatrix[k*4+j];
            }
        }
    }

    // Extract frustum planes using proper plane equation extraction
    // Each plane is defined as ax + by + cz + d = 0
    
    // Left plane: clip[3] + clip[0]
    {
        double a = clipMatrix[3] + clipMatrix[0];
        double b = clipMatrix[7] + clipMatrix[4];
        double c = clipMatrix[11] + clipMatrix[8];
        double d = clipMatrix[15] + clipMatrix[12];
        
        YsVec3 normal(a, b, c);
        double len = normal.GetLength();
        if (len > YsTolerance) {
            normal = normal / len;
            d = d / len;
            // Convert plane equation to point-normal form
            YsVec3 origin = normal * (-d);
            frustum.planes[0].Set(origin, normal);
        } else {
            frustum.planes[0].Set(YsVec3::Origin(), YsVec3(1.0, 0.0, 0.0));
        }
    }

    // Right plane: clip[3] - clip[0]
    {
        double a = clipMatrix[3] - clipMatrix[0];
        double b = clipMatrix[7] - clipMatrix[4];
        double c = clipMatrix[11] - clipMatrix[8];
        double d = clipMatrix[15] - clipMatrix[12];
        
        YsVec3 normal(a, b, c);
        double len = normal.GetLength();
        if (len > YsTolerance) {
            normal = normal / len;
            d = d / len;
            YsVec3 origin = normal * (-d);
            frustum.planes[1].Set(origin, normal);
        } else {
            frustum.planes[1].Set(YsVec3::Origin(), YsVec3(-1.0, 0.0, 0.0));
        }
    }

    // Bottom plane: clip[3] + clip[1]
    {
        double a = clipMatrix[3] + clipMatrix[1];
        double b = clipMatrix[7] + clipMatrix[5];
        double c = clipMatrix[11] + clipMatrix[9];
        double d = clipMatrix[15] + clipMatrix[13];
        
        YsVec3 normal(a, b, c);
        double len = normal.GetLength();
        if (len > YsTolerance) {
            normal = normal / len;
            d = d / len;
            YsVec3 origin = normal * (-d);
            frustum.planes[2].Set(origin, normal);
        } else {
            frustum.planes[2].Set(YsVec3::Origin(), YsVec3(0.0, 1.0, 0.0));
        }
    }

    // Top plane: clip[3] - clip[1]
    {
        double a = clipMatrix[3] - clipMatrix[1];
        double b = clipMatrix[7] - clipMatrix[5];
        double c = clipMatrix[11] - clipMatrix[9];
        double d = clipMatrix[15] - clipMatrix[13];
        
        YsVec3 normal(a, b, c);
        double len = normal.GetLength();
        if (len > YsTolerance) {
            normal = normal / len;
            d = d / len;
            YsVec3 origin = normal * (-d);
            frustum.planes[3].Set(origin, normal);
        } else {
            frustum.planes[3].Set(YsVec3::Origin(), YsVec3(0.0, -1.0, 0.0));
        }
    }

    // Near plane: clip[3] + clip[2]
    {
        double a = clipMatrix[3] + clipMatrix[2];
        double b = clipMatrix[7] + clipMatrix[6];
        double c = clipMatrix[11] + clipMatrix[10];
        double d = clipMatrix[15] + clipMatrix[14];
        
        YsVec3 normal(a, b, c);
        double len = normal.GetLength();
        if (len > YsTolerance) {
            normal = normal / len;
            d = d / len;
            YsVec3 origin = normal * (-d);
            frustum.planes[4].Set(origin, normal);
        } else {
            frustum.planes[4].Set(YsVec3::Origin(), YsVec3(0.0, 0.0, 1.0));
        }
    }

    // Far plane: clip[3] - clip[2]
    {
        double a = clipMatrix[3] - clipMatrix[2];
        double b = clipMatrix[7] - clipMatrix[6];
        double c = clipMatrix[11] - clipMatrix[10];
        double d = clipMatrix[15] - clipMatrix[14];
        
        YsVec3 normal(a, b, c);
        double len = normal.GetLength();
        if (len > YsTolerance) {
            normal = normal / len;
            d = d / len;
            YsVec3 origin = normal * (-d);
            frustum.planes[5].Set(origin, normal);
        } else {
            frustum.planes[5].Set(YsVec3::Origin(), YsVec3(0.0, 0.0, -1.0));
        }
    }

    return frustum;
}

double FsCullingUtil::DistanceToPlane(const YsPlane &plane, const YsVec3 &point)
{
    const YsVec3 &normal = plane.GetNormal();
    const YsVec3 &origin = plane.GetOrigin();

    // Calculate signed distance from point to plane
    // Using the standard point-to-plane distance formula
    double distance = normal * (point - origin);

    return distance;
}

YSBOOL FsCullingUtil::IsPointVisible(const Frustum &frustum, const YsVec3 &point, const double tolerance)
{
    for (int i = 0; i < 6; i++) {
        if (DistanceToPlane(frustum.planes[i], point) < -tolerance) {
            return YSFALSE;
        }
    }
    return YSTRUE;
}

YSBOOL FsCullingUtil::IsSphereVisible(const Frustum &frustum, const YsVec3 &center, const double radius, const double tolerance)
{
    // Check against each frustum plane
    for (int i = 0; i < 6; i++) {
        // Only cull if fully outside the frustum (with tolerance)
        if (DistanceToPlane(frustum.planes[i], center) < -(radius + tolerance)) {
            return YSFALSE;
        }
    }
    return YSTRUE;
}

YSBOOL FsCullingUtil::IsBoundingBoxVisible(const Frustum &frustum, const YsVec3 bbx[2], const double tolerance)
{
    // Test all 8 corners of the box
    YsVec3 corners[8];
    corners[0].Set(bbx[0].x(), bbx[0].y(), bbx[0].z());
    corners[1].Set(bbx[1].x(), bbx[0].y(), bbx[0].z());
    corners[2].Set(bbx[0].x(), bbx[1].y(), bbx[0].z());
    corners[3].Set(bbx[1].x(), bbx[1].y(), bbx[0].z());
    corners[4].Set(bbx[0].x(), bbx[0].y(), bbx[1].z());
    corners[5].Set(bbx[1].x(), bbx[0].y(), bbx[1].z());
    corners[6].Set(bbx[0].x(), bbx[1].y(), bbx[1].z());
    corners[7].Set(bbx[1].x(), bbx[1].y(), bbx[1].z());

    // For each plane, check if all points are outside
    for (int i = 0; i < 6; i++) {
        int outsideCount = 0;
        for (int j = 0; j < 8; j++) {
            if (DistanceToPlane(frustum.planes[i], corners[j]) < -tolerance) {
                outsideCount++;
            }
        }

        // If all points are outside this plane, the box is not visible
        if (outsideCount == 8) {
            return YSFALSE;
        }
    }

    return YSTRUE;
}
