# daq-wireless-com
Ensuring reliability in communication protocols is hard. As programmers, we seek to abstract away logic in order to (hopefully) provide clarity.
The logic NFR's Wireless Board's firmware has been complex enough to warrant the abstraction of an integral part of the project - namely the implementation of an application and transport layer communication protocol.

This repository provides an abstraction of these communication protocols, and rigorous-software based testing of the communication protocols through the simulation of a lossy communication channel in a busy enviornment. By pulling this code out of the main firmware repository, we intend to use a more-declaritive programming style in the main loops of the wireless firmware, whilst providing reusability for future years.

Keep posted for updates on this library, including more detailed documentation and an overview of the relatively custom protocols.
