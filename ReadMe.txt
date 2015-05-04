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
4a. TextureOptimize
    ミップマップを適用し、Defaultヒープに転送したテクスチャを描きます。
    SampleLevel()で小さいミップレベルを指定しています。
    Draw a texture with mipmap and transferred to default heap.
    Set small mip level by SampleLevel().

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
7b. MeshTexBundle
    バンドルを使って描画します。
    Draw using Bundle.

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


*** Environment ***

Windows 10 Technical Preview Build 10074
Visual Studio Community 2015 RC
Standalone Windows SDK for Windows 10



*** Test ***

1. VirtualBox on Windows 8.1 (WDDM1.3)
2. Radeon R7 240 (WDDM2.0)



*** Author ***

@shobomaru
しょぼまる


英語難しい
Sorry for my poor English.
