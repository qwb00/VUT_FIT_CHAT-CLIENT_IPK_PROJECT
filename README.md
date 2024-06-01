# IPK24-CHAT Client Project

## 1. Introduction

The main goal of the project is to develop and implement a chat application, which is able to communicate with a remote server using the IPK24-CHAT protocol. The protocol has two variants - each built on top of a different transport protocol. The first variant uses a TCP transport protocol that allows for a reliable communication channel, ensuring that messages are delivered in the order they were sent and without loss. The second variant utilizes the UDP transport protocol, which offers faster message delivery by foregoing the overhead of ensuring order and reliability. UDP’s connectionless communication model allows for quick data transmission without the need for establishing and maintaining a connection, making it efficient for broadcasting messages to multiple recipients.

### Program Execution

You can compile the program with the command `make` and execute it with `./ipk24chat-client`. The execution of the program supports the following arguments:

- `-t [tcp—udp]` Choose the transport protocol to use.
- `-s [address]` Server IP address or hostname.
- `-p [port]` Port number (default is 4567).
- `-d [ms]` Confirmation timeout for UDP (default is 250ms).
- `-r [count]` Maximum number of UDP retransmissions (default is 3).
- `-h` Prints help for program execution.

In Table 1, there are commands that the user can use in the client.

| Command         | Parameters             | Behavior                                    |
|-----------------|------------------------|---------------------------------------------|
| /auth           | Username Secret DisplayName | Sends authentication message to the server  |
| /join           | ChannelID              | Requests server to join the channel with ChannelID |
| /rename         | DisplayName            | Change user display name                    |
| /help           |                        | Writes the help message with all commands   |

## 2. Implementation Details

The program’s operation is governed by an FSM (Finite State Machine), which assists in efficiently managing the communication process between the client and the server. By defining a clear set of states and transitions based on incoming and outgoing messages, the FSM ensures that the program behaves predictably in response to various events. This structured approach facilitates handling complex interactions, such as authentication, message exchange, and error management, by guiding the client through a series of well-defined steps.

For better functioning, two global structures are used: `user` and `mc` (message content). The `user` structure contains all information about the user, such as username, display name, and password. It also has time values, retries, and message ID for UDP implementation. The `message content` structure is used for easy information handling, which is sent by the server.

### 2.1 TCP Implementation

TCP operates as follows: when a user wishes to establish a connection using the TCP transport protocol, the TCP client function is initiated. This function creates a socket, establishes a connection with the server, and initiates the operation of the FSM. The FSM transitions to the `auth` state and starts the `fsm auth state TCP` function. This function awaits the user to input the `/auth` command with the correct parameters and receive confirmation from the server that the user has been successfully authenticated and connected to the default channel (general).

Following successful authentication, the FSM transitions to the `OPEN` state. If an error occurs, the connection to the server is terminated, and the client is disconnected. In the `OPEN` state, the core logic of operation is enacted. The FSM executes the logic contained within the `fsm open state tcp` function. Handling incoming messages while the user is typing a message was resolved using the `select` function, which allows for parallel checking of messages from the server and stdin. Upon receiving a message from the server, the `handle incoming message` and `parse tcp input` functions parse this message and execute logic according to the message content. In reading mode from stdin, user commands such as `/join`, `/rename`, and `/help` are checked, and logic corresponding to these commands is executed. If none of these commands are recognized, it indicates that the user has sent a message, which is then sent to the server in the specified TCP format.

### 2.2 UDP Implementation

UDP implementation works similarly to TCP but with a few changes. The `construct message` function constructs a message to be sent, and the `send UDP message` function sends the message and waits for confirmation from the server. The UDP implementation addresses protocol issues like packet loss and packet delay/duplication. The solution for packet loss involves confirmation timeouts and retries. After the client sends a message, it waits for the server's confirmation. If it waits too long, the message is resent, and the function waits again for confirmation. By default, the client has 3 attempts (1 initial + 3 retries) to get confirmation, with intervals of 250 ms between attempts. After all retries, the client sends an error and bye message to the server.

To handle packet delay/duplication, a message ID is used. The message ID is a 2-byte number sent to the server with each message and incremented with each new message. This improves UDP's reliability by addressing common issues like missing messages and delays.

One of the most challenging aspects of the UDP implementation was implementing dynamic ports. After an `auth` message is sent to the default port 4567 (or user-provided port), the next message will be written and saved in the global `user` structure with the server address variable. All subsequent messages will be sent and received with this new information.

## 4. Conclusion

The IPK24-CHAT client project aimed to develop a chat application leveraging TCP and UDP protocols. It offered a deep dive into the intricacies of network communications. Successfully integrating TCP for reliable, ordered message delivery and UDP for fast, broadcast capabilities, the project demonstrated the effectiveness of using both protocols in harmony. A Finite State Machine (FSM) facilitated structured communication flow, ensuring smooth user interaction and reliable message exchange. Challenges such as adapting UDP to enhance reliability through confirmation timeouts and message IDs were overcome, showcasing innovative problem-solving. Extensive testing, including local simulations and public server interactions, validated the application’s performance and robustness. This project not only enhanced my understanding of network protocols but also highlighted the importance of testing and innovation in software development.

## 5. Sources

- Catch CTRL+C in C: [Stack Overflow](https://stackoverflow.com/questions/4217037/catch-ctrl-c-in-c)
- Programming network applications: [Moodle](https://moodle.vut.cz/pluginfile.php/823898/mod_folder/content/0/IPK2023-24L-04-PROGRAMOVANI.pdf)
- setsockopt function: [POSIX](https://pubs.opengroup.org/onlinepubs/000095399/functions/setsockopt.html)
- RFC 1350 - The TFTP Protocol: [RFC Editor](https://datatracker.ietf.org/doc/html/rfc1350#autoid-4)
