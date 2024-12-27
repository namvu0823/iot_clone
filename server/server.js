const express = require('express');
const bodyParser = require('body-parser');
const WebSocketHandler = require('./websocket');
const { router, initializeRouter } = require('./routes');

// Khởi tạo Express app
const app = express();
const HTTP_PORT = 3000;
const WS_PORT = 8080;

// Khởi tạo WebSocket Server cho ESP32
const wsHandler = new WebSocketHandler(WS_PORT);

// Cấu hình middleware
app.use(bodyParser.json());
app.use(express.static('public'));

// Khởi tạo router với WebSocket handler
initializeRouter(wsHandler);

// Sử dụng router
app.use('/', router);

// Khởi động HTTP server
app.listen(HTTP_PORT, () => {
    console.log(`HTTP Server running at http://localhost:${HTTP_PORT}`);
});