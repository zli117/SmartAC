var net = require('net');
var randomstring = require("randomstring");

var counter = 0;
var queue = require('fixed-size-queue').create(60);

var h, t, t_set, on = false, set = false, data_avail = false;

var server = net.createServer(function(socket) {
    
    socket.on('data', function(data){
        var data_strs = data.toString().trim().split(',');
        if (data_strs[0] === "join") {
            var id = randomstring.generate(7)
            socket.write(id);
            console.log(id);
            return;
        }

        if (data_strs[0] === "act") {
            
            if (t > t_set && !on && data_avail && set) {
                socket.write('0');            // 0: turn on
                on = true;
                console.log("turn on");
                return;
            } 

            if (t < t_set && on && data_avail && set) {
                socket.write('1');            // 1: turn off
                on = false;
                console.log("turn off");
                return;
            }

            socket.write('2');                // 2: hold
            console.log("hold");
            return;
        }

        if (data_strs[0] === "set") {
            console.log("set temperature");
            t_set = parseFloat(data_strs[1]);
            set = true;
            return;
        }

        if (data_strs[1] === "nan") {
            counter += 1;
            return;
        }
        id= data_strs[0]
        t = parseFloat(data_strs[1]);
        h = parseFloat(data_strs[2]);
        data_avail = true;
        console.log('Temperature = ' + t.toString() + ' humidity = ' + h.toString() + ' ID = ' + id + ' #nan: ' + counter.toString() + '\n');
    });

    socket.on('close', function(){
        // nothing here
    });

    socket.on('end', function(){
        // nothing here
    });
});

server.listen(12290, '192.168.1.103');