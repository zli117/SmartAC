from __future__ import print_function
import SocketServer
import collections
import random

class MyServer(SocketServer.TCPServer):

    class RequestHandler(SocketServer.BaseRequestHandler):

        def handle(self):
            data = self.request.recv(1024).strip().split(',')

            if len(data) < 1:
                return

            if len(data) == 1 and data[0] == "join":
                id = ''.join(random.choice(string.ascii_uppercase + string.digits) for _ in range(7))
                self.request.send(id)
                print("new process joined with id %s" % id)
                return

            if len(data) == 1 and data[0] == "act":

                if len(self.server.t_queue) > 0 and self.server.t_set != None:
                    average = sum(self.server.t_queue) / float(len(self.server.t_queue))

                    if average > (self.server.t_set + 1) and not self.server.on:
                        self.request.send("0") # 0: turn on
                        self.server.on = True
                        print("turned on")
                        return

                    if average < (self.server.t_set - 1) and self.server.on:
                        self.request.send("1") # 1: turn off
                        self.server.on = False
                        print("turned off")
                        return

                self.request.send("2") # 2: hold
                return

            if len(data) == 2 and data[0] == "set":
                try:
                    self.server.t_set = float(data[1])
                except Exception as e:
                    print(e)
                print("set temperature to %f" % self.server.t_set)
                return

            if len(data) == 2:

                if data[1] == "nan":
                    return

                id = data[0]
                self.server.t_queue.append(float(data[1]))
                self.server.h_queue.append(float(data[1]))

    def __init__(self, server_address, handler_class=RequestHandler, bind_and_activate=True, smooth_len=60):
        SocketServer.TCPServer.__init__(self, server_address, handler_class, bind_and_activate)
        self.h_queue = collections.deque([], smooth_len)
        self.t_queue = collections.deque([], smooth_len)
        self.on = False
        self.t_set = None

if __name__ == '__main__':
    server = MyServer(("192.168.1.103", 12290))
    server.serve_forever()



           
