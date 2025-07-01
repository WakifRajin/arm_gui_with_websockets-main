# server.py - Enhanced Version
import asyncio
import websockets
import json
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

clients = set()

async def handler(websocket):
    global clients  # Add this line to fix the scope issue
    client_addr = websocket.remote_address
    logger.info(f"New client connected: {client_addr}")
    clients.add(websocket)
    
    try:
        async for message in websocket:
            try:
                # Validate JSON if needed
                if message.startswith('{') or message.startswith('['):
                    json.loads(message)  # Validate JSON format
                
                logger.info(f"Broadcasting message from {client_addr}: {message}")
                
                # Broadcast to all other clients
                disconnected = set()
                for client in clients.copy():
                    if client != websocket:
                        try:
                            await client.send(message)
                        except websockets.exceptions.ConnectionClosed:
                            disconnected.add(client)
                
                # Clean up disconnected clients
                clients -= disconnected
                
            except json.JSONDecodeError:
                logger.warning(f"Invalid JSON from {client_addr}")
            except Exception as e:
                logger.error(f"Error processing message: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client {client_addr} disconnected normally")
    except Exception as e:
        logger.error(f"Error in handler for {client_addr}: {e}")
    finally:
        clients.discard(websocket)
        logger.info(f"Client {client_addr} removed from clients list")

async def main():
    logger.info("Starting WebSocket server on port 8765")
    async with websockets.serve(handler, "0.0.0.0", 8765):
        logger.info("✅ WebSocket server running on port 8765")
        await asyncio.Future()  # Run forever

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        logger.info("Server shutdown requested")
    except Exception as e:
        logger.error(f"Server error: {e}")