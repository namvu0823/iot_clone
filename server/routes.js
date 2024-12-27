const express = require('express');
const router = express.Router();

let wsHandler;

const initializeRouter = (websocketHandler) => {
    wsHandler = websocketHandler;
};

// Route tạo tài khoản mới
router.post('/create-account', (req, res) => {
    const { uid, password } = req.body;
    const message = {
        header: 'Create new account',
        payload: { UID: uid, password }
    };

    wsHandler.setPendingResponse(uid, res);
    wsHandler.sendToESP32(message);
    console.log("gửi yêu cầu tạo vân tay tới esp32");

    // Timeout sau 30 giây
    setTimeout(() => {
        if (wsHandler.hasPendingResponse(uid)) {//nếu sau 30s esp32 không phản hồi thiod báo lỗi
            wsHandler.deletePendingResponse(uid);
            res.json({
                status: 'error',
                message: 'Request timeout'
            });
        }
    }, 30000);
});

// Route xử lý check-in
router.post('/check-in', (req, res) => {
    const { UID, password, finger } = req.body;
    
    const message = {
        header: 'Check_in response',
        payload: {
            UID: UID,
            password: password,
            finger: finger
        }
    };
   
    wsHandler.setPendingResponse(UID, res);
    wsHandler.sendToESP32(message);
    console.log("Gửi yêu cầu check-in tới ESP32");

    // Timeout sau 60 giây
    setTimeout(() => {
        if (wsHandler.hasPendingResponse(UID)) {
            wsHandler.deletePendingResponse(UID);
            res.json({
                status: 'error',
                message: 'Check-in request timeout'
            });
        }
    }, 60000);
});

// Route cho trang chủ
router.get('/', (req, res) => {
    res.sendFile(process.cwd() + '/public/client.html');
});

module.exports = { router, initializeRouter };