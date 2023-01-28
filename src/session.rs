//SOCKS5 session

use std::net::SocketAddr;

use tokio::net::TcpStream;

pub async fn enter(stream : TcpStream, remote_addr : SocketAddr) {
    log::debug!("New SOCKS5 connection from {}", remote_addr);

    //SOCKS5 handshake
    
    //Find address to connect to/UDP associate

    //Pull address from load balancer

}


