//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#pragma once
#include "../core/stdafx.h"

using namespace DirectX;

class Camera
{
public:
    Camera();
    ~Camera();


    void Get3DViewProjMatricesLH(XMFLOAT4X4 *view, XMFLOAT4X4 *proj, float fovInDegrees, float screenWidth, float screenHeight);
    void Get3DViewProjMatrices(XMFLOAT4X4 *view, XMFLOAT4X4 *proj, float fovInDegrees, float screenWidth, float screenHeight, float nearZ, float farZ);
    void Reset();
    void Set(XMVECTOR eye, XMVECTOR at, XMVECTOR up);
    static Camera *get();
    void RotateAroundYAxis(float angleRad);
    void RotateYaw(float angleRad);
    void RotatePitch(float angleRad);
    void GetOrthoProjMatrices(XMFLOAT4X4 *view, XMFLOAT4X4 *proj, float width, float height);
    void Move(bool wKeyPressed, bool sKeyPressed, bool aKeyPressed, bool dKeyPressed, float moveDistance);
    XMVECTOR mEye; // Where the camera is in world space. Z increases into of the screen when using LH coord system (which we are and DX uses)
    XMVECTOR mAt; // What the camera is looking at (world origin)
    XMVECTOR mUp; // Which way is up
private:
    static Camera* mCamera;
};
