import os, sys, logging, asyncio
import argparse
from SoraC import SoraC, Sink
#import numpy as np
#import cv2

# check if data from this track is necessary
# return True to START track
# track_kind: 'audio' or 'video'
def cb_check_track(client_id, track_kind):
    # sample:
    # return client_id == 'your_target' and track_kind == 'video'
    return True

# called when data arrives
def cb_data(client_id, track_kind, data, datainfo):
    # datainfo sample:
    # audio: {'bits_per_sample': '16', 'sample_rate': '48000', 'number_of_channels': '1', 'number_of_frames': '480', 'track_id': 'xxx'}
    # video: {'width': '640', 'height': '480', 'channels': '4', 'track_id': 'xxx'}

    if datainfo is None:
        print('datainfo is None:', client_id, track_kind)
        return
    if track_kind == 'video':
        # # data is raw image of ARGB format
        # # convert to OpenCV image (numpy array of (h, w, 4)
        # data_np = np.frombuffer(data, np.uint8)
        # img = data_np.reshape((datainfo['height'],
        #                        datainfo['width'],
        #                        datainfo['channels'],
        #                        ))
        # # do something with OpenCV
        # cv2.imshow('test', img)
        # cv2.waitKey(0)
        pass

    elif track_kind == 'audio':
        # # data is raw PCM format
        bits_per_sample = datainfo['bits_per_sample']
        sample_rate = datainfo['sample_rate']
        number_of_channels = datainfo['number_of_channels']
        number_of_frames = datainfo['number_of_frames']
        # do something with audio data

try:
    from dotenv import load_dotenv
    load_dotenv()
except:
    print('WARNING: python-dotenv not found')

if __name__ == '__main__':
    # show all logs on console
    root = logging.getLogger()
    root.setLevel(logging.DEBUG)
    handler = logging.StreamHandler(sys.stdout)
    handler.setLevel(logging.DEBUG)
    formatter = logging.Formatter('%(asctime)s %(levelname)-8s %(name)s %(message)s')
    handler.setFormatter(formatter)
    root.addHandler(handler)

    parser = argparse.ArgumentParser()
    parser.add_argument('channel_id', type=str)
    parser.add_argument('--prog_path', type=str, default='_build/ubuntu-22.04_x86_64/release/sora_recv/sora_recv')
    parser.add_argument('--signaling_url', type=str, default=os.getenv("SORA_SIGNALING_URL"))
    parser.add_argument('--metadata', type=str, default=os.getenv("SORA_METADATA"))
    args = parser.parse_args()

    soraC = SoraC(cb_check_track, cb_data,
                  channel_id=args.channel_id,
                  signaling_url=args.signaling_url,
                  metadata=args.metadata,
                  prog_path=args.prog_path,
                  )
    soraC.start()

    loop = asyncio.get_event_loop()
    try:
        loop.run_forever()
    finally:
        soraC.stop()
        loop.close()
