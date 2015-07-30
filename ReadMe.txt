*** HelloD3D12 ***

Direct3D 12を使った簡単なサンプルです。
Sample source code using Direct3D 12.

ゆっくりしていってね！！！
Enjoy!



*** Projects ***

1. Window Present
    ウィンドウを作ってクリアします。
    Create and clear a window.

2. Shader
    頂点シェーダとピクセルシェーダで全画面を塗りつぶします。
    Draw the entire window using a vertex shader and pixel shader.
        (based on 1. Window Present)
2a. Fullscreen
    [Not worked on Radeon (Acccess violation).]
    全画面に切り替えます。
    Switch fullscreen mode.

3. VertexBuffer
    頂点バッファとインデックスバッファを使って描画します。
    2つのバッファを同じ1つのGPUヒープにまとめて確保します。
    Draw using a vertex buffer and index buffer.
    Allocate one GPU heap and store two buffers.
        (based on 2. Shader)

4. Texture
    テクスチャを描画します。
    Draw a texture.
        (based on 2. Shader)
    [Not worked on HARDWARE device.]
4a. TextureOptimize
    ミップマップを適用し、Defaultヒープに転送したテクスチャを描きます。
    SampleLevel()で小さいミップレベルを指定しています。
    BC圧縮テクスチャを扱えます。
    Draw a texture with mipmap and transferred to default heap.
    Set small mip level by SampleLevel().
    Support BC compressed texture format.

5. ConstantBuffer
    定数バッファとテクスチャを1つのDescriptorHeapにまとめて描画します。
    Draw using a constant buffer and texture combined in one descriptor heap.
        (based on 4. Texture)

6. DepthBuffer
    深度バッファに描いたものを表示します。
    Draw depth buffer and display that.
        (based on 3. VertexBuffer + 4. Texture)

7. Mesh
    ユタ・ティーポットを描きます。
    Draw the Utah teapot.
        (based on 3. VertexBuffer + 5. ConstantBuffer + 6. DepthBuffer)
7a. MeshTex
    テクスチャつきのティーポットを描きます。
    UVは適当です。
    Draw the Utah teapot with a texture.
        (based on 4. Texture)
    [Not worked on HARDWARE device.]
7b. MeshTexBundle
    バンドルを使って描画します。
    Draw using Bundle.
    [Not worked on HARDWARE device.]

8. ParallelFrame
    コマンドリストを複数使い、フレームの終わりでCPUをストールさせないようにします。
    Use multiple command lists in order to avoid CPU stall at the end of rendering frame.
        (based on 3. Mesh)
8a. ParallelFrameRootConstant
    RootConstantを使い、毎フレームDescriptorTableの切り替えをしないようにします。
    Use root constant in order to avoid changing descriptor table every frame.

9. Multithread
    マルチスレッドでコマンドリストを構築します。
    Build command lists with multithreaded.
        (based on 8. ParallelFrame)

10. ExecuteIndirect
    [Not worked on Radeon (Removing device).]
    一部のパラメータとドローコールの引数をVRAMに書き込み、バッチを間接的に実行します。
    Write a part of parameters and arguments of drawcall, and execute batches indirectly.
        (based on 8.ParallelFrame)

11. Readback
    ComputeShaderからのメッセージを読み取ります。
    Read message from ComputeShader.

12. Placement
    異なるリソースを一つのヒープから切り出して作成します。
    Create different resources which is cut dowm from one heap.

13. UpdateConstantEmu
    Direct3D 11のUpdateSubresourceと同等の処理を実装します。
    Implement equivant of UpdateSubresource().

14. D3D11Interop
    Direct3D 12デバイスからDirect3D 11デバイスを作って描画します。
    Create and draw Direct3D 11 device from Direct3D 12 device.

15. PSOCache
    PipelineStateをシリアライズし、ファイルに保存します。
    Serialize pipeline state and save to file.


*** Environment ***

Windows 10 Home/Pro 10240
Visual Studio Community 2015 RTM
Windows 10 SDK (10240)
-- https://dev.windows.com/en-us/downloads/windows-10-developer-tools


*** Test ***

1. Intel HD Graphics 4400 (WDDM2.0)
2. AMD Radeon R7 240 (WDDM2.0)



*** Author ***

@shobomaru
しょぼまる


英語難しい
Sorry for my poor English.
