# socket-chat

## Private message

@[name] [message]發送訊息給[name]

-user查詢目前連線的名稱

## multithreading

server為每個連線的client創造一個thread，提供訊息接收與廣播服務
![socket multithreading-第 1 页 drawio](https://github.com/user-attachments/assets/a315cb43-8add-48d3-8d5f-bed2da975d74)

## IO multiplexing (epoll)為多個thread提供訊息接收與廣播服務
![socket epoll-2 drawio](https://github.com/user-attachments/assets/d3fe072b-2972-48f2-8792-1335f8b9188d)
