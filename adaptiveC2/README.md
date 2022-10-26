# NOTES - READ BEFORE DIGGING INTO SLACK EXTERNAL C2
The core logic behind using an external C2 is to hide your payload output inside legitimate network traffic. This can be done in numerous ways using fronted domains, Dns Over Https or known redirectors such as aws/azure. However sometimes they are not enough. Sometimes you need something more subtle so that you can camouflage yourself into the 

1. SMB or TCP badgers can be used to interact with your External C2 Servers
2. The current example uses SMB badger which listens on the named pipe `\\.\pipe\mynamedpipe` which is fully configurable via badger's Payload Profile
3. All badgers return output which is encrypted and then encoded in base64
4. In the current example, our aim is to write a connector which reads output from the named pipe and sends it as a the request to the External C2 Server
5. Once a request is sent, our connector will also have to receive a response from the External C2 Server and then forward it over the same named pipe to the badger
6. If there is no response received from the External C2 Server, then we have to send a single byte "" to our named pipe to let the named pipe know there is no response yet and then continue to listen on the named pipe
7. Badger will frequently connect and send a request on the named pipe every 2 second which is the default sleep cycle unless changed.
7. External C2 connectors and servers can be written in any language. The current example uses C language since it's easy to convert the connector to a PIC as explained in my blog [here](https://bruteratel.com/research/feature-update/2021/01/30/OBJEXEC/)


## Slack C2 - Configuration

1. Slack -> Build -> New App -> Name of the App. eg: AdaptiveC2
2. Slack -> Build -> New App -> Name of the App. eg: BadgerApp
3. OAuth & Permissions -> Redirect URLs -> https://evasionlabs.com
4. OAuth & Permissions -> Scopes -> Bot Token Scopes ->
  - app_mentions:read
  - channels:history
  - channels:read
  - chat:write
  - groups:history
  - im:history
  - im:read
  - mpim:history
  - mpim:read
5. Install to Workspace (generates OAuth Tokens automatically)
6. App Home -> Messages Tab -> Allow users to send Slash commands and messages from the messages tab
7. Event Subscriptsions -> Enable -> https://evasionlabs.com
  - Activate via Challenge response
  - Subscribe to bot events
    - app_mentions
    - message.channels
    - message.groups
    - message.im
    - message.mpim

## Slack Connector

3. The output messages are encrypted and then base64 encoded before sending it across to the server or across pivot badgers (SMB and TCP)
4. This means when a badger sends a full message, it needs to be received in full by the Ratel Server. Ratel Server does not handle partial messages. If partial messages are received by the Ratel server, it cannot decode and decrypt the message
5. So, when using external C2, it is important to understand the limitation of your external c2 server and find out the maximum length of buffer it can accept
6. Slack accepts a maximum of `4000 bytes` per message, excluding the json parts [chat.postMessage](https://api.slack.com/methods/chat.postMessage)
7. So, we have to write a slack-connector which reads the full output msg from the badger. If the output size is >4000, split the buffer into chunks and send it across to our ListenerApp on slack.com
  - Slack-connector will send the the first buffer for around `4000 bytes or less` and prepend it with a uniqueBuffer before sending it, which implies its a partial message. We will call this unique value as `partialMessageDetector`
  - Upon receiving the first buffer, ListenerApp will return a `timestamp` for the first buffer
  - Slack-connector will extract the `timestamp` from the response and store it in memory
  - Slack-connector will send the remaining chunks of the output buffer in similar 4000 bytes or lower as replies to the first message. These replies will state that these buffers are part of the main message (first buffer)
  - ListenerApp will store the main message and replies and forward them via callbacks to our External C2 connector `Adaptivec2.py` written in Python3
  - Adaptivec2 will receive the first message as the first callback and extract the `timestamp` and the buffer. If the message contains `partialMessageDetector`, it means there are more messages in the replies. It will store this message in a dictionary
  - Subsequent callbacks received by the Adaptivec2 server from the ListenerApp will be used to identify the replies using `timestamp` and `partialMessageDetector` and append all of them as they are received
  - After receiving every callback event, Adaptivec2 will also delete each of the replies as soon as they are received.
  - If the final reply received by Adaptivec2 does not contain a `partialMessageDetector`, it means its the last part of the message
  - Adaptivec2 will concatenate all of them and forward the message as a single buffer to our Ratel server.
7. The Ratel server will send a response back to Adaptivec2
8. Adaptivec2 has to check if the response from the Ratel server is more than 4000 bytes. When commands like `sharpreflect`, `psreflect` or any shellcode/reflective DLLs are used, the buffer size is usually in kilobytes which is sent over the network
9. Adaptivec2 will check the buffer size, split it across into multiple chunks as messages and replies (similar to what the slack-connector did) and send it to our BadgerApp
10. The Slack-connector will send a https request to the BadgerApp and fetch the response. It will check if the response contains the same `partialMessageDetector` and start extract the replies similar to what the slack-connector did earlier.
11. Upon receipt of the final reply, it will concatenate all the messages, run the requested command and send a response back in the same way.

### Badger Configuration - SMB Payload
1. Badger sends main post request buffer to Slack C2 App
2. Slack C2 App sends it to a Python3 Server
3. Python3 Server forwards the request to BRc4 and deletes the message from the Slack C2 App
4. BRc4 sends response a to Python3 Server
5. Python3 Server forwards the request to a Slack Badgers App
6. Badger also sends another request to fetch the latest slack messages from the Badgers App
7. Badger gets the json response and filters it with:
  - starts with "text": "<@U039KRC46BS> 
    - U039KRC46BS is the Member ID of the Badger's App
  - ends with double quotes
  eg.: "text": "<@UB39KRC46B8> z6FVoM/jWEDXZdF2V427xsJ8pw3D0pcNK3ApkNOD/AceRj4vBoJdIQL1GKRXMB4H"
8. Badger deletes the messages in the Badger's App as per the `timestamp` (ts) of the message received above
