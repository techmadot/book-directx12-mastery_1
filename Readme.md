# DirectX12 Mastery Vol.1

本リポジトリは 「DirectX12 Mastery Vol.1」書籍のサンプルプログラムを提供しています。
不具合の修正等により、書籍の内容と若干変わっていることがあります。ご了承ください。

# ビルドについて

本リポジトリに含まれるサンプルプログラムは、 Visual Studio 2022 でビルドを確認しています。

Developer Command Prompt for VS 2022を開き、Commonフォルダ内のPrepareAssimp.batを実行してください。
その後、各サンプルの sln を開いてビルド・実行してください。

### 注意事項

Assimpのビルドにおいて、CMakeを使用します。
CMakeをインストールするか、Visual Studio 2022のインストール時にCMakeの機能を追加しているか、どちらかの準備が必要です。


# 免責事項

本リポジトリはサンプルコードの提供であり、ビルドや動作を保証するものではありません。
利用する場合には、利用者の責任において使用してください。

本リポジトリに含まれるモデルデータなどのアセットについては配布元の条件に従ってください。

- [glTF-Sample-Assets](https://github.com/KhronosGroup/glTF-Sample-Assets)
- [ニコニ立体 アリシア・ソリッド公式サイト](https://3d.nicovideo.jp/alicia/)

# バージョン情報

Submoduleで登録しているライブラリは以下のバージョンに固定しています。

- Dear ImGui : 1.91.0
- Assimp : 5.3.1

また、シングルヘッダライブラリとして STBより、stb_image.h と stb_image_resize.h を使用しています。
