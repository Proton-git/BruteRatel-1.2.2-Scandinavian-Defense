#!/usr/bin/python3

from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
import json
from time import sleep
import requests
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

# click on the Apps name in the chats section and get the last value from the URL to get the channel ID
APP_INFO_DICT = {
    "D03AFAZC6B1": "Bearer xoxb-2144924547920-3382587054001-2xPrUBj0D8yf0D5BNDPh3nwY", # Listener
    "D03AT6A4X9T": "Bearer xoxb-2144924547920-3393858142400-qEIcN8hBt0WgwRaJImILqMAj" # Commander
}

def FetchFullMessage(channel, token, msg_ts):
    threadMessageStart = "part$"
    threadMessageEnd = "$end"
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
            fullBadgerMessage = fullBadgerMessage + value.split(threadMessageStart)[1]
    except Exception as ex:
        print("[-] Exception reading replies:", ex)
    print(fullBadgerMessage)

def deleteMessageReplies(APP_CHANNEL_ID, APP_TOKEN, ts):
    requestUri = "https://slack.com/api/conversations.replies?channel="+ APP_CHANNEL_ID +"&ts=" + ts
    response = requests.get(requestUri, headers={'Authorization': APP_TOKEN})
    response = response.text
    jdata = json.loads(response)
    msgArray = []
    msgCount = 0
    for i in jdata['messages']:
        msgArray.append(i['ts'])
        msgCount+=1
    print("%d replies found" % msgCount)

    for thread_ts in msgArray:
        requestUri = "https://slack.com/api/chat.delete?channel="+ APP_CHANNEL_ID +"&ts=" + thread_ts
        requests.get(requestUri, headers={'Authorization': APP_TOKEN})
        sleep(0.5)
    print("All threads deleted")


def main():
    print("[+] Checking abandoned messages")
    for APP_CHANNEL_ID, APP_TOKEN in APP_INFO_DICT.items():
        try:
            requestUri = "https://slack.com/api/conversations.history?channel="+ APP_CHANNEL_ID +"&pretty=1"
            response = requests.get(requestUri, headers={'Authorization': APP_TOKEN})
            response = response.text
            jdata = json.loads(response)
            msgArray = []
            msgCount = 0
            for i in jdata['messages']:
                msgArray.append(i['ts'])
                msgCount+=1
            print("%d messages found" % msgCount)

            for ts in msgArray:
                deleteMessageReplies(APP_CHANNEL_ID, APP_TOKEN, ts)
                # delete the main message after deleting the replies
                requestUri = "https://slack.com/api/chat.delete?channel="+ APP_CHANNEL_ID +"&ts=" + ts
                response = requests.get(requestUri, headers={'Authorization': APP_TOKEN})
            print("All messages deleted")

        except Exception as ex:
            print("[!] Exception sending msg:", ex)

if __name__ == "__main__":
    main()
    # FetchFullMessage("D03AFAZC6B1", "Bearer xoxb-2144924547920-3382587054001-2xPrUBj0D8yf0D5BNDPh3nwY", "1651914125.900769")
