const WebSocket = require('ws');

class WebSocketHandler {
    constructor(port) {
        this.wss = new WebSocket.Server({ port });
        this.pendingResponses = new Map();
        this.initialize();
    }

    initialize() {
        console.log(`WebSocket Server running on port ${this.wss.options.port}`);

        this.wss.on('connection', (ws) => {
            console.log('ESP32 connected');

            ws.on('message', (message) => {
                this.handleMessage(message);
            });

            ws.on('close', () => {
                console.log('ESP32 disconnected');
            });
        });
    }

    handleMessage(message) {
        try {
            const data = JSON.parse(message);
            
            if (data.header === 'response new account') {
                this.handleNewAccountResponse(data);
            } else if (data.header === 'Check_in response') {
                this.handleCheckInResponse(data);
            }
        } catch (error) {
            console.error('Error handling message:', error);
        }
    }

    handleNewAccountResponse(data) {
        const { UID, status, finger } = data;
        const res = this.pendingResponses.get(UID);
        
        if (res) {
            if (status === 'successfully') {
                console.log("tạo vân tay thành công");
                console.log("Finger template:", finger);
                res.json({
                    status: 'success',
                    finger: finger
                    
                });
            } else {
                res.json({
                    status: 'error',
                    message: 'Failed to create fingerprint'
                });
            }
            this.pendingResponses.delete(UID);
        }
    }

    handleCheckInResponse(data) {
        const { UID, status } = data;
        const res = this.pendingResponses.get(UID);
        
        if (res) {
            if (status === 'successfully') {
                res.json({
                    status: 'success',
                    message: 'Check-in successful'
                });
            } else {
                res.json({
                    status: 'error',
                    message: 'Check-in failed'
                });
            }
            this.pendingResponses.delete(UID);
        }
    }

    sendToESP32(message) {
        this.wss.clients.forEach(client => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(JSON.stringify(message));
            }
        });
    }

    setPendingResponse(uid, res) {
        this.pendingResponses.set(uid, res);
    }

    deletePendingResponse(uid) {
        this.pendingResponses.delete(uid);
    }

    hasPendingResponse(uid) {
        return this.pendingResponses.has(uid);
    }
}

module.exports = WebSocketHandler;