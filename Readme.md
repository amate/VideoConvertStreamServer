# VideoConvertStreamServer

![](https://raw.githubusercontent.com/amate/VideoConvertStreamServer/images/images/screenshot4.png)

## ■はじめに
このソフトは、PC上にある動画ファイルをHLS形式に変換し、ブラウザを通じてスマホやタブレットで見るために作った簡易HTTPサーバーです

## ■動作環境
・Windows 10 home 64bit バージョン 1903  
※64bit版OSでしか動作しません (同伴しているffmpegの都合上)

## ■導入方法

起動したら設定から "ポート番号"、"パスワード"、"ルートフォルダ"を設定してください

※注意  
このプログラムは、LAN内の端末からのアクセスを想定して作られました  
一応パスワード保護は付けていますが、インターネットに公開することは想定していません  
設定したポート番号が、ルーターによって<b>解放されていない</b>ことを必ず確認してください

"ポート番号"を変更した場合は、プログラムの再起動が必要です

"ルートフォルダ"以下にある動画ファイルがサーバーで公開されます   
ルートフォルダは複数登録できないので、複数のドライブにまたがってフォルダを公開したい場合は、 
適当なルートフォルダにシンボリックリンクを置いてください

設定が終了したら、メインダイアログに書いてあるURLをクリックすれば既定のブラウザでローカルサイトが表示されるはずです

もし繋がらない場合は、コマンドプロンプトからipconfigを実行してPCのIPアドレスを確認して  
http://(表示されたIPv4 アドレス):ポート番号/  
を代わりに開いてみてください

http://127.0.0.1:ポート番号/  
それか上のURLで繋がるはず…

![](https://raw.githubusercontent.com/amate/VideoConvertStreamServer/images/images/screenshot1.png)

この画面が表示されればOK

あとは[root]をクリックして好きな動画を見てください

![](https://raw.githubusercontent.com/amate/VideoConvertStreamServer/images/images/screenshot2.png)
![](https://raw.githubusercontent.com/amate/VideoConvertStreamServer/images/images/screenshot3.png)

## ■設定

- ライブラリに表示する動画の拡張子  
ここに書かれた拡張子を持つファイルだけがサーバーに公開されます

- 動画を変換せずに、直接ブラウザに渡す動画の拡張子  
ここに書かれた拡張子を持つファイルは、動画再生にHLS形式に変換されません

- 動画変換に利用するエンジン  
デフォルトは同伴しているffmpegで変換しますが、設定したビットレートに対して画質があまりよくないので、他のffmpegや NVEncCを利用できます

- コマンドライン引数  
動画変換時に、エンコーダーに渡されるコマンドライン引数です  
\<input>は入力ファイル  
\<segmentFolder>は出力先フォルダに置換されます  
.tsファイル変換時は、\<DeinterlaceParam>にインターレース解除用のコマンドが書き込まれます  
GUIは用意していないので、Config.json の DeinterlaceParam を手動で変更してください


## ■使用ライブラリ・素材

Server  
- boost  
https://www.boost.org/

- JSON for Modern C++  
https://github.com/nlohmann/json

- ffmpeg  
https://ffmpeg.org/  
https://github.com/lembryo/ffmpeg  

- WTL  
http://sourceforge.net/projects/wtl/

html  
- bootstrap  
https://getbootstrap.com/

- Font Awesome  
https://fontawesome.com/v4.7.0/

- jQuery  
https://jquery.org/

icon
- ICOOON MONO  
https://icooon-mono.com/

## ■ビルド方法
Visual Studio 2019 が必要です  
ビルドには boost(1.72~)とWTL(10_9163) が必要なのでそれぞれ用意してください。

boostは事前にライブラリのビルドが必要になります

Boostライブラリのビルド方法  
https://boostjp.github.io/howtobuild.html  
//コマンドライン  
b2.exe install --prefix=lib64 toolset=msvc-14.2  runtime-link=static address-model=64 --with-log --with-filesystem --with-coroutine

◆boost  
http://www.boost.org/

◆WTL  
http://sourceforge.net/projects/wtl/

## ■著作権表示
Copyright (C) 2020 amate

私が書いた部分のソースコードは、MIT License とします。

## ■更新履歴

<pre>

v1.0
・公開

</pre>