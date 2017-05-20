using System;
using System.Collections.Generic;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace HardwareController {

    public class Controller {

        private SerialPort port;

        private void FindPort() {

            string[] portNames = SerialPort.GetPortNames();
            foreach (var portName in portNames) {
                port = new SerialPort(portName, 9600);
                

            }
        }



        private void SendCommand() {

        }


    }
}
