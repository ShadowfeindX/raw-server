#include <headers> //<- Please don't look in here

// Wrapper's wrapper
class Connection : public QObject
{
    Q_OBJECT
    /*
     * What is an HTTP communication?
     * 1. Socket on which to communicate
     * 2. Command saying what is to be done
     * 3. Headers defining how it is to be done
     * 4. Body of data to be used
     **/
  public:
    Connection(Server * server):
      Object(server) {
      socket = server->nextPendingConnection();
      socket->setParent(this);
    }
    ~Connection() {} // <- Maybe cleanup some stuff?
    // TODO
    Socket * socket;
    StrL command;
    Map<Str,Str> headers;
    Bytes body;
};

#include <main.moc>
int main(int argc, char *argv[])
{
  // Make a loop...thingy
  // Actually this process command line args
  // And runs async
  // AND manages a thread pool
  // I could really replace it with a while loop....
  // Nah
  Core loop(argc, argv);
  // Pointer to the server
  // No I will not put this on the stack
  // I might need to delete it while it's running
  // Ok I won't do that but it's too late I already put the * there
  Server * server = new Server(&loop);
  // Pointer to the log
  // Totally safe
  // No really
  File * log = new QFile("server.log", server);

  // Start listening for web connections
  server->listen(QHostAddress(QHostAddress::LocalHost),80);
  qDebug() << server // <_ Doxx yourself
           << "is listening on port" << server->serverPort()
           << "at address" << server->serverAddress();
  // Ok start listening for console cammands
  // Helpful debugging tool
  // Broke as shit
  // TODO
  QtConcurrent::run([server](){
    Str cmd;
    Map<Str,int> commands {
      {"quit",-1},
      {"children",0}
    };
    do {
      qin >> cmd;
      switch (commands.value(cmd.simplified().toCaseFolded())) {
        case 0:
          qDebug() << server->children();
          break;
        case -1:default:
          cmd.clear();
      }
    } while (!cmd.isEmpty());
    ((Core *)server->parent())->exit(0); //<- Please don't tell my mother
  });
  // Test Json for slash
  // Need to implement a helpful format to this garbage converter
  // TODO
  JObject index {
    {"name","Leanne Graham"},
    {"username","Bret"},
    {"email","Sincere@april.biz"},
    {"address", JObject {
        {"street","Kulas Light"},
        {"suite","Apt. 556"},
        {"city","Gwenborough"},
        {"zipcode","92998-3874"},
        {"geo", JObject {
            {"lat","-37.3159"},
            {"lng","81.1496"}
          }
        }
      }
    },
    {"phone","1-770-736-8031 x56442"},
    {"website","hildegard.org"},
    {"company",JObject{
        {"name","Romaguera-Crona"}
      }
    },
    {"catchPhrase","Multi-layered client-server neural-net"},
    {"bs","harness real-time e-markets"}
  };
  // Map for GET
  // Need like 4 more of these
  // OR maybe another map
  // TODO
  Map<Str,JObject> GET {
    {"/", index}
  };
  // No this is not the main loop
  Object::connect(server,&Server::newConnection,[=](){
    // Make a wrapper to the wrapper to the socket
    Connection * con = new Connection(server);
    qDebug() << con //<- Doxx the clients
             << "has connected on port" << con->socket->peerPort()
             << "at address" << con->socket->peerAddress();
    // Also not the loop
    Object::connect(con->socket,&Socket::readyRead,[=](){
      // Tell the world we finally started reading these dammed bytes
      qDebug() << "Reading" << con->socket->bytesAvailable() << "bytes...";
      auto logData = [log](Bytes data){
        log->open(QIODevice::WriteOnly | QIODevice::Append);
        log->write(data);
        log->close();
      };
      auto parseHeaders = [con](){
        con->command = Str(con->socket->readLine()).simplified().split(" ");
        Str data;
        while (!(data = Str(con->socket->readLine()).simplified().remove(" ")).isEmpty()) {
          // Yeah ok some devil magic that turns malformed strings into fairy princesses
          StrL _header = data.split(":");
          con->headers.insert(_header.first(),_header.last());
        }
      };
      if (-1 != QString(con->socket->peek(con->socket->bytesAvailable())).indexOf("\r\n\r\n")) {
        // This nonsesne here ^^^
        // Pull all the available readable bytes into memory
        // Look for a blank line
        // Assume everything above that line is header data
        // and everything below is binary trash

        // If this connection is new and hasn't been given a cammand
        // Go get one from the headers
        // If it already has a command.....idk how the heck we got here
        // Log it and keep moving
        if (con->command.isEmpty()) {
          parseHeaders(); // I'm sure you can read
          con->socket->write("HTTP/1.1 200 OK\r\n");
          // Maybe I should actually add some other response codes...
          // TODO

          // Ok each command is a 3 part list
          // 1. Command itself i.e. HEAD, GET, POST, etc.
          // 2. Location i.e. /, /index, /api
          // 3. HTTP version wholly ignored at this point <- TODO
          if (con->command.takeFirst() == "GET") {
            // Takes the first part of the list and checks for GET
            // List size is now 2
            // Probably should check for some other stuff....
            // TODO

            Bytes response;
            JObject _response = GET[con->command.takeFirst()];
            // Ok taking the next part and checking it against my GET map
            // Creates a default null json object if nothing turns up
            // Again probably more of these need to be here

            // If we didn't get a real request tell em to scram
            if (_response.isEmpty()) {
              response = "You got what you came fer.\r\n"
                         "Now Scram!\r\n";
              con->socket->write(Str("Content-Type: text/plain\r\n"
                                     "Content-Length: %1\r\n" // <- Replaced by devil magic
                                     "\r\n")
                                 .arg(response.length()) // <- Devil magic
                                 .toUtf8()); // <- 8 bit unicode conversion
              con->socket->write(response); // <- Honestly these should probably be encoded or something at least
              // Very late TODO
            } else {
              // Alright we got a cool poll to our shit api

              // Format some JSON with nice dents and all
              // Really screws with chrome's parser
              // Too bad
              response = Json(_response).toJson(Json::Indented);
              con->socket->write(Str("Content-Type: application/json\r\n"
                                     "Content-Length: %1\r\n" //<- Again with the satanism
                                     "\r\n")
                                 .arg(response.length())
                                 .toUtf8());
              con->socket->write(response);
            }
          } con->socket->waitForBytesWritten(); // <- apparently this randomly fails
          con->socket->disconnectFromHost(); // <- Not sure if this is more graceful than close
          // But the name is longer so let's assume so
        } else {
          logData(con->socket->readAll()); // <- logging bit I mentioned like 50 lines ago
        }
      }
    }); // Still not the loop
    Object::connect(con->socket,&Socket::stateChanged,[=](){
      // Check for...really? Can't say disconnected? Whatever
      if (con->socket->state() == QAbstractSocket::UnconnectedState) {
        con->deleteLater(); // <- Delete on disconnect
      } else {
        // Something that's not disconnect...
        qDebug() << con->socket->state(); // <- Log that shit
      }
    });
  });
  return loop.exec(); // <- OK NOW we start the damn event loop
}
