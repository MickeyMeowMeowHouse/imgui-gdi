// dear imgui: Renderer for GDI (Based on emilk's imgui_software_renderer.)
// This needs to be used along with a Platform Binding (e.g. Win32)

// Implemented features:
//  [X] Renderer: Support for large meshes (64k+ vertices) with 16-bits indices.
// Issues:
//  [ ] You can't use User texture binding because of the imgui_software_renderer don't support that.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you are new to dear imgui, read examples/README.txt and read the documentation at the top of imgui.cpp
// https://github.com/ocornut/imgui

// CHANGELOG
//  2019-08-12: GDI: Improve the implementation of the GDI renderer.
//  2019-08-12: GDI: Add Windows GDI Renderer Support.

#include "imgui.h"
#include "imgui_impl_gdi.h"

#define NOMINMAX
#include <Windows.h>

#include <vector>

static int old_fb_width = 0;
static int old_fb_height = 0;

//static HDC g_hDC = nullptr;
//static HDC g_hBufferDC = nullptr;
//static HBITMAP g_hBitmap = nullptr;
//static uint32_t* g_PixelBuffer = nullptr;
//static size_t g_PixelBufferSize = 0;
static HBRUSH g_BackgroundColorBrush = nullptr;

bool ImGui_ImplGDI_Init()
{
    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_gdi";

    // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    return true;
}

void ImGui_ImplGDI_Shutdown()
{
    
}

void ImGui_ImplGDI_NewFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)(intptr_t)0;
}

void ImGui_ImplGDI_SetBackgroundColor(ImVec4* BackgroundColor)
{
    if (g_BackgroundColorBrush)
    {
        DeleteObject(g_BackgroundColorBrush);
        g_BackgroundColorBrush = nullptr;
    }

    g_BackgroundColorBrush = CreateSolidBrush(RGB(
        BackgroundColor->x * 256.0,
        BackgroundColor->y * 256.0,
        BackgroundColor->z * 256.0));
}

std::vector<TRIVERTEX> g_VertexBuffer;
std::vector<GRADIENT_TRIANGLE> g_MeshBuffer;

#include <cstdint>

// Render function
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(),
// but you can now call this directly from your main loop)
void ImGui_ImplGDI_RenderDrawData(ImDrawData* draw_data)
{
    // Avoid rendering when minimized, scale coordinates for retina displays.
    // (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width == 0 || fb_height == 0)
        return;

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Get the handle of the current window.
    ImGuiIO& io = ImGui::GetIO();
    HWND hWnd = (HWND)io.ImeWindowHandle;

    HDC hDC = GetDC(hWnd);

    HDC hBufferDC = CreateCompatibleDC(hDC);

    HBITMAP hBufferBitmap = CreateCompatibleBitmap(
        hDC,
        draw_data->DisplaySize.x,
        draw_data->DisplaySize.y);

    //unsigned char* pixels;
    //int width, height;
    //io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    //// Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.

    //BITMAPINFO FontBitmapInfo;

    //FontBitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    //FontBitmapInfo.bmiHeader.biWidth = width;
    //FontBitmapInfo.bmiHeader.biHeight = height;
    //FontBitmapInfo.bmiHeader.biPlanes = 1;
    //FontBitmapInfo.bmiHeader.biBitCount = 32;
    //FontBitmapInfo.bmiHeader.biCompression = BI_RGB;

    //HBITMAP hFontBitmap = CreateDIBSection(hBufferDC, &FontBitmapInfo, DIB_RGB_COLORS, (void**)&pixels, NULL, 0);

    //    //CreateBitmap(width, height, 1, 1, pixels);

    ////HBRUSH hFontBrush = CreatePatternBrush(hFontBitmap);

    //SelectObject(hBufferDC, hFontBitmap);

    HBITMAP hBitmap = (HBITMAP)SelectObject(hBufferDC, hBufferBitmap);

    if (g_BackgroundColorBrush)
    {
        RECT rc;
        rc.left = 0;
        rc.top = 0;
        rc.right = fb_width;
        rc.bottom = fb_height;

        FillRect(hBufferDC, &rc, g_BackgroundColorBrush);
    }

    for (int n = 0; n < draw_data->CmdListsCount; ++n)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];

        g_VertexBuffer.clear();
        g_MeshBuffer.clear();

        for (int i = 0; i < cmd_list->VtxBuffer.Size; ++i)
        {
            TRIVERTEX Vertex;
            Vertex.x = cmd_list->VtxBuffer[i].pos.x;
            Vertex.y = cmd_list->VtxBuffer[i].pos.y;
            Vertex.Red = ((uint8_t*)&cmd_list->VtxBuffer[i].col)[0] << 8;
            Vertex.Green = ((uint8_t*)&cmd_list->VtxBuffer[i].col)[1] << 8;
            Vertex.Blue = ((uint8_t*)&cmd_list->VtxBuffer[i].col)[2] << 8;
            Vertex.Alpha = ((uint8_t*)&cmd_list->VtxBuffer[i].col)[3] << 8;

            g_VertexBuffer.push_back(Vertex);
        }

        for (int i = 0; i < cmd_list->IdxBuffer.Size; i += 3)
        {
            GRADIENT_TRIANGLE Mesh;
            Mesh.Vertex1 = cmd_list->IdxBuffer[i];
            Mesh.Vertex2 = cmd_list->IdxBuffer[i + 1];
            Mesh.Vertex3 = cmd_list->IdxBuffer[i + 2];

            g_MeshBuffer.push_back(Mesh);
        }

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                pcmd->UserCallback(cmd_list, pcmd);
            }
            else
            {
                // Bind texture

                // Draw
                /*GdiGradientFill(
                    hBufferDC,
                    &g_VertexBuffer[pcmd->VtxOffset],
                    g_VertexBuffer.size() - pcmd->VtxOffset,
                    &g_MeshBuffer[pcmd->IdxOffset],
                    g_MeshBuffer.size() - pcmd->IdxOffset,
                    GRADIENT_FILL_TRIANGLE);*/

                GdiGradientFill(
                    hBufferDC,
                    &g_VertexBuffer[0],
                    g_VertexBuffer.size(),
                    &g_MeshBuffer[0],
                    g_MeshBuffer.size(),
                    GRADIENT_FILL_TRIANGLE);

                break;
            }
            
        }
    }

    BitBlt(
        hDC,
        0,
        0,
        draw_data->DisplaySize.x,
        draw_data->DisplaySize.y,
        hBufferDC,
        0,
        0,
        SRCCOPY);

    //DeleteObject(hFontBitmap);
    //DeleteObject(hFontBrush);

    DeleteObject(hBufferBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hBufferDC);
    ReleaseDC(hWnd, hDC);
}
