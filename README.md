# sora_recv

[sora-cpp-sdk-samples](https://github.com/shiguredo/sora-cpp-sdk-samples)の
momo_sample を改造したもの。
Sora のチャネルにrecvonly で入り、指定されたtrackId のデータをstdout に吐き出す。track が無くなる(クライアントが接続を切るなど）と終了する。現状では音声トラックにのみ対応。対話内容を音声認識にかけるなどの用途で使う。

Ubuntu22のみを抜き出してある。

# 作り方
`sudo apt install pkg-config libva-dev libdrm-dev libx11-dev libxext-dev`

sora-cpp-sdk のexamples/ でsora_recv をgit clone し、
`python sora_recv/run.py` でコンパイルすると、_build/ の下にバイナリができる。


## 注意

WSLでWindows ファイルシステム上でビルドしようとすると、webrtc のライブラリがWindows Defender などのウィルスチェックに引っかかり、止まるので注意。

# usage

```
> ./sora_recv --help
Momo Sample for Sora C++ SDK
Usage: ./sora_recv [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --log-level INT:value in {verbose->0,info->1,warning->2,error->3,none->4} OR {0,1,2,3,4}
                              Log severity level threshold
  --signaling-url TEXT REQUIRED
                              Signaling URL
  --channel-id TEXT REQUIRED  Channel ID
  --client-id TEXT            Client ID
  --metadata TEXT:JSON Value  Signaling metadata used in connect message
  --proxy-url TEXT            Proxy URL
  --proxy-username TEXT       Proxy username
  --proxy-password TEXT       Proxy password
  --track-id TEXT REQUIRED    track id to output data
```


# 音声出力

出力はs16le 48k, 1chのraw フォーマット。
```
./sora_recv ... > test.raw
ffmpeg -f s16le -ar 48k -ac 1 -i test.raw test.wav
```

# stderr

stderr にはもともと(momo_sample) のstdout + stderr の出力が出る(`dup()` している)とともに、AddRemoteStream, NotificationMessage などのイベントに対応したメッセージがJSONで出力される。これらは`sora_recv:` が行頭につくので、それで識別できる。

以下のような感じで出てくるので、Notification とTrack からclient_id に対応するstream_id, stream_id に対応するtrack_id とkind を見ることで、取り出したいtrack_id を得ることもできる。
ターゲットのclient_id とkind を指定すると、それに対応するtrack を検出し、その中身を出し続けるようにしてもいいかもしれない。

```json
sora_recv: Notify: {"audio":true,"channel_connections":1,"channel_recvonly_connections":1,"channel_sendonly_connections":0,"channel_sendrecv_connections":0,"client_id":"0RNTMZSXJX4K52H4HMS85BB85M","connection_id":"0RNTMZSXJX4K52H4HMS85BB85M","data":[],"event_type":"connection.created","minutes":0,"role":"recvonly","session_id":"EB3A4ER59H6K53KJSVVG2Z4G74","turn_transport_type":"udp","type":"notify","video":true}
sora_recv: Track: {"id":"DDTG9JT4J97H19THK7F31E9PXC","kind":"audio","streams":["EWFHYP453X6KDFBJGWNGYRZPMM"]}

sora_recv: Track: {"id":"21QQ5S8AF91693YK5CZ2R83STW","kind":"video","streams":["EWFHYP453X6KDFBJGWNGYRZPMM"]}

sora_recv: Notify: {"audio":true,"channel_connections":2,"channel_recvonly_connections":1,"channel_sendonly_connections":0,"channel_sendrecv_connections":1,"client_id":"CLIENT2","connection_id":"EWFHYP453X6KDFBJGWNGYRZPMM","event_type":"connection.created","minutes":0,"role":"sendrecv","session_id":"EB3A4ER59H6K53KJSVVG2Z4G74","turn_transport_type":"udp","type":"notify","video":true}
sora_recv: Track: {"id":"XV4C2607KD3V59VJ44CS2HZF34","kind":"audio","streams":["52YYX8NKSS2D59A27DF526A3R0"]}

sora_recv: Track: {"id":"4HXNTVGG7S56H36Q00J2WKMCG8","kind":"video","streams":["52YYX8NKSS2D59A27DF526A3R0"]}

sora_recv: Notify: {"audio":true,"channel_connections":3,"channel_recvonly_connections":1,"channel_sendonly_connections":0,"channel_sendrecv_connections":2,"client_id":"CLIENT1","connection_id":"52YYX8NKSS2D59A27DF526A3R0","event_type":"connection.created","minutes":0,"role":"sendrecv","session_id":"EB3A4ER59H6K53KJSVVG2Z4G74","turn_transport_type":"udp","type":"notify","video":true}
Session Initialization Time: 139 ms
sora_recv: RemoveTrack: {"id":"DDTG9JT4J97H19THK7F31E9PXC","kind":"audio","streams":""}

sora_recv: RemoveTrack: {"id":"21QQ5S8AF91693YK5CZ2R83STW","kind":"video","streams":""}

sora_recv: Notify: {"audio":true,"channel_connections":2,"channel_recvonly_connections":1,"channel_sendonly_connections":0,"channel_sendrecv_connections":1,"client_id":"CLIENT2","connection_id":"EWFHYP453X6KDFBJGWNGYRZPMM","event_type":"connection.destroyed","minutes":0,"role":"sendrecv","session_id":"EB3A4ER59H6K53KJSVVG2Z4G74","turn_transport_type":"udp","type":"notify","video":true}
Session Deinitialization Time: 14 ms
sora_recv: RemoveTrack: {"id":"XV4C2607KD3V59VJ44CS2HZF34","kind":"audio","streams":""}

sora_recv: RemoveTrack: {"id":"4HXNTVGG7S56H36Q00J2WKMCG8","kind":"video","streams":""}

sora_recv: Notify: {"audio":true,"channel_connections":1,"channel_recvonly_connections":1,"channel_sendonly_connections":0,"channel_sendrecv_connections":0,"client_id":"CLIENT1","connection_id":"52YYX8NKSS2D59A27DF526A3R0","event_type":"connection.destroyed","minutes":0,"role":"sendrecv","session_id":"EB3A4ER59H6K53KJSVVG2Z4G74","turn_transport_type":"udp","type":"notify","video":true}
```

