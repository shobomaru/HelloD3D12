*** HelloD3D12 ***

Direct3D 12を使った簡単なサンプルです。
Sample source codes using Direct3D 12.

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



*** Environment ***

Windows 10 Technical Preview Build 10041
Visual Studio 2015 CTP 6
Visual Studio Tools for Windows 10



*** Test ***

1. VirtualBox on Windows 8.1 (WDDM1.3)
2. Radeon R7 240 (WDDM2.0)

WARPでもHARDWAREでも動きました。



*** Author ***

@shobomaru
しょぼまる


英語難しい
Sorry for my poor English.
