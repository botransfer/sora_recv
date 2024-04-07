# sora_recv

[sora-cpp-sdk](https://github.com/shiguredo/sora-cpp-sdk)のmomo_sample を改造したもの。
Sora のチャネルにrecvonly で入り、audio またはvideo track のデータを出力する。
顔認識をしたり、音声認識にかけるなどの用途で使う。

Ubuntu22のみを抜き出してある。

# テスト環境
- Ubuntu 22.04.3 LTS
- Python 3.10.12
- sora-cpp-sdk 2024.6.0

# 作り方

```
sudo apt install pkg-config libva-dev libdrm-dev libx11-dev libxext-dev
git clone https://github.com/shiguredo/sora-cpp-sdk.git
cd sora-cpp-sdk/examples
git clone https://github.com/botransfer/sora_recv.git
python sora_recv/run.py
```

## 注意

WSLでWindows ファイルシステム上でビルドしようとすると、webrtc のライブラリがWindows Defender などのウィルスチェックに引っかかり、止まるので注意。WSL用のファイルシステム上ならOK。

# usage

```
> _build/ubuntu-22.04_x86_64/release/sora_recv/sora_recv --help
sora receiver
Usage: _build/ubuntu-22.04_x86_64/release/sora_recv/sora_recv [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --signaling-url TEXT REQUIRED
                              Signaling URL
  --channel-id TEXT REQUIRED  Channel ID
  --client-id TEXT            Client ID
  --metadata TEXT:JSON Value  Signaling metadata used in connect message
```

# 入出力

基本的にpython script などで使うことを想定。`receiver_test.py` を参照。
`cb_check_track()` と`cb_data()` を書き換えればいいと思う。重い処理はmutiprocessing を使いましょう。

標準出力にSora からのnotify, track, removeTrack などのメッセージが出力されるので、それをみて
必要なtrack が来たら、（sora_recv の標準入力に）コマンドを送って、named pipe にデータを出させる。
named pipe はmkdtemp() で作られたディレクトリ内に作成される。

## コマンド

- `START <track_id> <named pipe などのパス>`:
  出力を開始する
- `STOP <track_id>`:
  出力を止める。removeTrack がSora から来ると勝手に止まる。
- `SHUTDOWN`:
  sora_recv を終了する。python script をCtrl-C で止めるとシグナルが送られるので、勝手に止まる。

# 問題点

sora-cpp-sdk がメモリリークしているようで、何もしていなくても少しずつメモリ使用量が増えていく。
