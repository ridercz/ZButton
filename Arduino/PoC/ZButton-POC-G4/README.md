# Z-Button Proof of Concept Gen. 4

This is permanently connected Z-Button. Connect a locking switch between D5 and ground. It will send one HTTPS request when pressed down and one when released. The query string is signed with HMACSHA256. Configuration is saved in SPIFFS and messages are sequentially numbered.