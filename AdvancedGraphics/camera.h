#pragma once


#include <DirectXMath.h>
#include <windows.h>
#include <windowsx.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

using namespace DirectX;

class Camera
{
public:
    Camera(XMFLOAT3 posIn, XMFLOAT3 lookDirIn, XMFLOAT3 upIn)
    {
        position = posIn;
        lookDir = lookDirIn;
        up = upIn;

        XMStoreFloat4x4(&viewMatrix, XMMatrixIdentity());
    }

    XMFLOAT3 GetPosition() { return position; }

    void MoveForward(float distance)
    {
        // Get the normalized forward vector (camera's look direction)
        XMVECTOR forwardVec = XMVector3Normalize(XMLoadFloat3(&lookDir));
        XMVECTOR posVec = XMLoadFloat3(&position);

        // Move in the direction the camera is facing
        XMStoreFloat3(&position, XMVectorMultiplyAdd(XMVectorReplicate(distance), forwardVec, posVec));
    }

    void StrafeLeft(float distance)
    {
        // Get the current look direction and up vector
        XMVECTOR lookDirVec = XMLoadFloat3(&lookDir);
        XMVECTOR upVec = XMLoadFloat3(&up);

        // Calculate the right vector (side vector of the camera)
        XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(upVec, lookDirVec));
        XMVECTOR posVec = XMLoadFloat3(&position);

        // Move left by moving opposite to the right vector
        XMStoreFloat3(&position, XMVectorMultiplyAdd(XMVectorReplicate(-distance), rightVec, posVec));
    }

    void MoveBackward(float distance)
    {
        // Call MoveForward with negative distance to move backward
        MoveForward(-distance);
    }

    void StrafeRight(float distance)
    {
        // Call StrafeLeft with negative distance to move right
        StrafeLeft(-distance);
    }


    void UpdateLookAt(POINTS delta)
    {
        // Sensitivity factor for mouse movement
        const float sensitivity = 0.001f;

        // Apply sensitivity
        float dx = delta.x * sensitivity; // Yaw change
        float dy = delta.y * sensitivity; // Pitch change


        // Get the current look direction and up vector
        XMVECTOR lookDirVec = XMLoadFloat3(&lookDir);
        lookDirVec = XMVector3Normalize(lookDirVec);
        XMVECTOR upVec = XMLoadFloat3(&up);
        upVec = XMVector3Normalize(upVec);

        // Calculate the camera's right vector
        XMVECTOR rightVec = XMVector3Cross(upVec, lookDirVec);
        rightVec = XMVector3Normalize(rightVec);



        // Rotate the lookDir vector left or right based on the yaw
        lookDirVec = XMVector3Transform(lookDirVec, XMMatrixRotationAxis(upVec, dx));
        lookDirVec = XMVector3Normalize(lookDirVec);

        // Rotate the lookDir vector up or down based on the pitch
        lookDirVec = XMVector3Transform(lookDirVec, XMMatrixRotationAxis(rightVec, dy));
        lookDirVec = XMVector3Normalize(lookDirVec);


        // Re-calculate the right vector after the yaw rotation
        rightVec = XMVector3Cross(upVec, lookDirVec);
        rightVec = XMVector3Normalize(rightVec);

        // Re-orthogonalize the up vector to be perpendicular to the look direction and right vector
        upVec = XMVector3Cross(lookDirVec, rightVec);
        upVec = XMVector3Normalize(upVec);

        // Store the updated vectors back to the class members
        XMStoreFloat3(&lookDir, lookDirVec);
        XMStoreFloat3(&up, upVec);

    }

    void Update() { UpdateViewMatrix(); }

    XMMATRIX GetViewMatrix() const
    {
        UpdateViewMatrix();
        return XMLoadFloat4x4(&viewMatrix);
    }



private:

    void UpdateViewMatrix() const
    {
        // Calculate the look-at point based on the position and look direction
        XMVECTOR posVec = XMLoadFloat3(&position);
        XMVECTOR lookDirVec = XMLoadFloat3(&lookDir);
        XMVECTOR lookAtPoint = posVec + lookDirVec; // This is the new look-at point

        // Update the view matrix to look from the camera's position to the look-at point
        XMStoreFloat4x4(&viewMatrix, XMMatrixLookAtLH(posVec, lookAtPoint, XMLoadFloat3(&up)));
    }

    XMFLOAT3 position;
    XMFLOAT3 lookDir;
    XMFLOAT3 up;
    mutable XMFLOAT4X4 viewMatrix;
};

