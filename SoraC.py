import os, sys, time, queue, logging
import threading, subprocess, uuid
import json
import tempfile, select
import struct, socket
import asyncio

class Sink:
    def __init__(self, tempdir, connection_id, track_id, track_kind, client_id, cb_data):
        self.connection_id = connection_id
        self.track_id = track_id
        self.track_kind = track_kind
        self.client_id = client_id
        self.cb_data = cb_data
        self.logger = logging.getLogger(f'Sink/{client_id}/{track_kind}')

        self.datainfo = None
        self.path_fifo = os.path.join(tempdir, track_id)
        try:
            os.mkfifo(self.path_fifo)
        except OSError as oe: 
            raise
        threading.Thread(target=self.reader).start()
        self.logger.info('started')

    # data info
    def set_datainfo(self, info):
        self.datainfo = info

    def get_path_fifo(self):
        return self.path_fifo

    def stop(self):
        if not self.running: return
        with open(self.path_fifo, 'w') as fifo:
            pass

    # data info
    def set_datainfo(self, info):
        self.datainfo = info

    def read_fifo(self, fifo):
        # read header
        res = select.select([fifo],[],[])
        header = os.read(fifo, struct.calcsize("I"))
        if len(header) < 1:
            return None
        len_remaining = struct.unpack("!I", header)[0]

        # read data
        data = None
        while len_remaining > 0:
            res = select.select([fifo],[],[])
            data_part = os.read(fifo, len_remaining)
            if len(data_part) < 1:
                return None
            if data is None:
                data = data_part
            else:
                data += data_part
            len_remaining -= len(data_part)
        return data

    def reader(self):
        self.running = True
        fifo = os.open(self.path_fifo, os.O_RDONLY | os.O_NONBLOCK)
        while True:
            data = self.read_fifo(fifo)
            if data is None: break
            self.cb_data(self.client_id, self.track_kind, data, self.datainfo)

        os.close(fifo)
        os.remove(self.path_fifo)
        self.running = False
        self.logger.debug('reader stopped')

class SoraC:
    def __init__(self, cb_check_track, cb_data):
        self.cb_check_track = cb_check_track
        self.cb_data = cb_data
        self.client_id = 'sora_recv/' + str(uuid.uuid4()) # random client ID
        self.map_client_id = dict()
        self.map_track_id = dict()
        self.map_sink = dict()
        self.tempdir = tempfile.mkdtemp()
        self.logger = logging.getLogger(self.__class__.__name__)
        
        proc_args = [
            '_build/ubuntu-22.04_x86_64/release/sora_recv/sora_recv',
            '--signaling-url', 'wss://sora2.botransfer.org/signaling',
            '--client-id', self.client_id,
            '--channel-id', 'r/19',
            '--metadata', '{"signaling_key": "Xpf2SnOkqLo4htyA7jQEuvhqNEMUD34wGlu8SPYodvC74l3M"}',
        ]
        self.process = subprocess.Popen(proc_args,
                                   stdout=subprocess.PIPE,
                                   stdin=subprocess.PIPE,
                                   encoding='utf8',
                                   )
        self.queue_send = queue.Queue()

    def start(self):
        threading.Thread(target=self.writer, args=[self.process.stdin,  self.queue_send]).start()
        threading.Thread(target=self.reader, args=[self.process.stdout]).start()

    def stop(self):
        self.queue_send.put(None)
        self.process.wait()
        os.rmdir(self.tempdir)

    def send(self, msg):
        self.queue_send.put(msg)

    def parse_msg(self, msg_type, msg):
        msg = json.loads(msg)
        if msg_type == 'notify':
            event_type = msg['event_type']
            connection_id = msg['connection_id']
            client_id = msg['client_id']
            data = msg['data'] if 'data' in msg else []
            if event_type == 'connection.created':
                if client_id == self.client_id:
                    # sora_recv connected
                    self.connection_id = connection_id
                    self.logger.info(f"connected to Sora: {client_id} {connection_id}")
                    for entry in data:
                        self.map_client_id[entry['connection_id']] = entry['client_id']
                        self.logger.info(f"existing client: {entry['client_id']} {entry['connection_id']}")
                        self.check_track(entry['connection_id'])
                else:
                    self.map_client_id[connection_id] = client_id
                    self.logger.info(f"new connection: {client_id} {connection_id}")
                    self.check_track(connection_id)

            elif event_type == 'connection.destroyed':
                del self.map_client_id[connection_id]
                del self.map_track_id[connection_id]
            
        elif msg_type == 'addTrack':
            track_id = msg['id']
            track_kind = msg['kind']
            connection_id = msg['streams'][0]
            if connection_id not in self.map_track_id:
                self.map_track_id[connection_id] = dict()
            self.map_track_id[connection_id][track_kind] = track_id
            self.check_track(connection_id)

        elif msg_type == 'removeTrack':
            track_id = msg['id']
            track_kind = msg['kind']
            self.map_sink[track_id].stop()
            del self.map_sink[track_id]

        elif msg_type == 'datainfo':
            track_id = msg['track_id']
            if track_id in self.map_sink:
                self.map_sink[track_id].set_datainfo(msg)

    def check_track(self, connection_id):
        if connection_id not in self.map_client_id:
            self.logger.info(f"client_id unknown yet: {connection_id}")
            return
        client_id = self.map_client_id[connection_id]
        if connection_id not in self.map_track_id:
            self.logger.info(f"track_id unknown yet: {connection_id}")
            return
        for track_kind in self.map_track_id[connection_id]:
            track_id = self.map_track_id[connection_id][track_kind]
            if track_id not in self.map_sink:
                if self.cb_check_track(client_id, track_kind):
                    sink = Sink(self.tempdir, connection_id, track_id, track_kind, client_id, self.cb_data)
                    self.map_sink[track_id] = sink
                    path_fifo = sink.get_path_fifo()
                    # send command to sora_recv
                    self.send(f"START {track_id} {path_fifo}")

    def writer(self, f, queue):
        for line in iter(queue.get, None):
            if line is None: break
            self.logger.info('sending: ' + line)
            f.write(line + "\n")
            f.flush()
        f.close()
        self.logger.info('writer thread stop')

    def reader(self, f):
        for line in f:
            line = line.rstrip()
            if len(line) == 0: continue
            if not line.startswith('sora_recv:'):
                self.logger.debug(line)
                continue
            _, msg_type, line = line.split(':', 2)
            self.logger.info(f"{msg_type}: {line}")
            if msg_type != 'log' and msg_type != 'ERR':
                self.parse_msg(msg_type, line)

        f.close()
        self.logger.info('reader thread stop')

