from __future__ import print_function
import SocketServer
import collections
import random
import threading
import argparse
import time

class MyServer(SocketServer.ThreadingMixIn, SocketServer.TCPServer):

    class RequestHandler(SocketServer.BaseRequestHandler):

        def handle(self):

            data = self.request.recv(1024).strip().split(",")

            if len(data) < 1:
                return

            if len(data) == 1 and data[0] == "join":
                id = "".join(random.choice(string.ascii_uppercase + string.digits) for _ in range(7))
                self.request.send(id)
                if self.server.verbose:
                    print("new process joined with id %s" % id)
                return

            if len(data) == 1 and data[0] == "act":

                if len(self.server.t_queue) > 0 and self.server.t_low:
                    average = sum(self.server.t_queue) / float(len(self.server.t_queue))
                    if self.server.verbose:
                        print("average = ", average, end=" ")

                    if self.server.t_high != None and average > self.server.t_high and not self.server.on:
                        self.request.send("0") # 0: turn on
                        self.server.on = True
                        if self.server.verbose:
                            print("turned on")
                        return

                    if self.server.t_low != None and average < self.server.t_low and self.server.on:
                        self.request.send("1") # 1: turn off
                        self.server.on = False
                        if self.server.verbose:
                            print("turned off")
                        return

                self.request.send("2") # 2: hold
                if self.server.verbose:
                    print("hold")
                return

            if len(data) == 3 and data[0] == "set":
                try:
                    if data[1] == "low":
                        self.server.t_low = float(data[2])
                    if data[1] == "high":
                        self.server.t_high = float(data[2])
                except Exception as e:
                    print(e)
                if self.server.verbose:
                    print("set temperature to (%f, %f)" % (self.server.t_low, self.server.t_high))
                return

            if len(data) == 3:

                if data[1] == "nan":
                    return

                id = data[0]
                t = float(data[1])
                h = float(data[2])
                if self.server.verbose:
                    print("id = %s, t = %f, h = %f" % (id, t, h))
                self.server.t_queue.append(t)
                self.server.h_queue.append(h)


    def __init__(self, server_address, handler_class=RequestHandler, bind_and_activate=True, smooth_len=60, verbose=False):
        self.h_queue = collections.deque([], smooth_len)
        self.t_queue = collections.deque([], smooth_len)
        self.on = False
        self.t_low = None
        self.t_high  = None
        self.allow_reuse_address = True
        self.verbose = verbose
        SocketServer.TCPServer.__init__(self, server_address, handler_class, bind_and_activate)

def parse_cmd_args():
    description = "Server for the SmartAC"  
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--verbose", action="store_true",
                        help="Set if you want to print the log on terminal")
    parser.add_argument("-ip", type=str, default="0.0.0.0",
                        help="ip address others can contact the server")
    parser.add_argument("-port", type=int,
                        help="The port number")
    return parser



if __name__ == "__main__":
    parser = parse_cmd_args()
    args = parser.parse_args()

    if args.ip == None or args.port == None:
        parser.print_help()
        exit(2)

    server = MyServer((args.ip, args.port), verbose=args.verbose)

    server.serve_forever()


           
