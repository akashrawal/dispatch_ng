/* session.rs
 * SOCKS5 session/protocol implementation
 * 
 * Copyright 2023 Akash Rawal
 * This file is part of dispatch_ng.
 * 
 * dispatch_ng is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * dispatch_ng is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with dispatch_ng.  If not, see <http://www.gnu.org/licenses/>.
 */

use std::borrow::Cow;

use std::net::{SocketAddr, IpAddr, Ipv4Addr};

use tokio::io::{AsyncRead, AsyncWrite, AsyncReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpStream, TcpSocket};

use crate::balancer::Balancer;

#[derive(Debug)]
enum Error {
    Read(std::io::Error),
    Write(std::io::Error),
    Copy(std::io::Error),
    Socket(std::io::Error),
    Bind(std::io::Error),
    Connect(std::io::Error),
    NameResolution(std::io::Error),
    Protocol(Cow<'static, str>),
    UnsupportedProtocolVersion(u8),
    UnsupportedAuthentication,
    UnsupportedCommand(u8),
    InvalidAddress(u8),
    DomainIsNotStrimg(std::string::FromUtf8Error),
    NoTargetAddr,
    NoSuitableSourceAddr,
}

/*
enum Command {
    Connect,
    Bind,
    UdpAssociate,
}
*/

#[derive(Debug)]
#[repr(u8)]
enum AddrType {
    IPv4 = 1,
    Domain = 3,
    IPv6 = 4,
}

#[repr(u8)]
enum ReplyCode {
    Success = 0,
    GeneralSocksServerFailure = 1,
    /*
    ConnectionNotAllowedByRuleset = 2,
    NetworkUnreachable = 3,
    HostUnreachable = 4,
    */
    ConnectionRefused = 5,
    TTLExpired = 6,
    CommandNotSupported = 7,
    AddressTypeNotSupported = 8,
}

pub async fn enter(
    stream : impl AsyncRead + AsyncWrite, 
    client_addr : SocketAddr,
    balancer : Balancer
) {
    log::debug!("{}: New SOCKS5 connection", client_addr);

    let result : Result<(), Error> = async {
        let (istream, mut ostream) = tokio::io::split(stream);
        let mut istream = BufReader::new(istream);

        //Authentication
        //Read VER, NMETHODS
        let mut auth_head = [0_u8; 2];
        istream.read_exact(&mut auth_head).await
            .map_err(Error::Read)?;
        if auth_head[0] != 5 {
            return Err(Error::UnsupportedProtocolVersion(auth_head[0]));
        }
        let nmethods = auth_head[1] as usize;
        if !(1..=255).contains(&nmethods) {
            return Err(Error::Protocol(
                format!("NMETHODS should be between 1 and 255 ({})", 
                        auth_head[1]).into()
                ));
        }
        let mut methods = vec![0_u8; nmethods]; 
        istream.read_exact(&mut methods).await
            .map_err(Error::Read)?;
        //TODO: Try to add password authentication
        if methods.contains(&0) {
            ostream.write_all(&[5_u8, 0_u8]).await
                .map_err(Error::Write)?;
        } else {
            ostream.write_all(&[5_u8, 255_u8]).await
                .map_err(Error::Write)?;
            ostream.shutdown().await
                .map_err(Error::Write)?;
            return Err(Error::UnsupportedAuthentication); //Close the connection
        }
        log::debug!("{}: Authentication complete", client_addr);

        //SOCKS5 request
        let maybe_target_stream : Result<TcpStream, Error> = async {
            //Read VER, CMD, RSV, ATYP
            let mut req_head = [0_u8; 4];
            istream.read_exact(&mut req_head).await
                .map_err(Error::Read)?;
            if req_head[0] != 5 {
                return Err(Error::UnsupportedProtocolVersion(req_head[0]));
            }
            if req_head[1] != 1 {
                //TODO: try to support UDP associate
                return Err(Error::UnsupportedCommand(req_head[1]));
            }
            if req_head[2] != 0 {
                return Err(Error::Protocol("RSV value must be 0.".into()))
            }
            
            //Find address to connect to/UDP associate
            let addr_type = match req_head[3] {
                1 => AddrType::IPv4,
                3 => AddrType::Domain,
                4 => AddrType::IPv6,
                _ => return Err(Error::InvalidAddress(req_head[3])),
            };
            log::debug!("{}: AddrType: {:?}", client_addr, addr_type);

            let addrlist = match addr_type {
                AddrType::IPv4 => {
                    let mut addr = [0_u8; 4];
                    istream.read_exact(&mut addr).await
                        .map_err(Error::Read)?;
                    vec![IpAddr::from(addr)]
                },
                AddrType::IPv6 => {
                    let mut addr = [0_u8; 16];
                    istream.read_exact(&mut addr).await
                        .map_err(Error::Read)?;
                    vec![IpAddr::from(addr)]
                },
                AddrType::Domain => {
                    let len = istream.read_u8().await
                        .map_err(Error::Read)?;
                    let mut bytes = vec![0_u8; len as usize];
                    istream.read_exact(&mut bytes).await
                        .map_err(Error::Read)?;

                    //Can tokio not lookup arbitrary sequence of bytes?
                    let domain = String::from_utf8(bytes)
                        .map_err(Error::DomainIsNotStrimg)?;

                    //DNS lookup
                    log::debug!("{}: Resolving {}", client_addr, domain);
                    tokio::net::lookup_host(domain + ":0").await
                        .map_err(Error::NameResolution)?
                        .map(|socketaddr| socketaddr.ip())
                        .collect()
                }
            };
            log::debug!("{}: addrlist: {:?}", client_addr, addrlist);

            if addrlist.is_empty() {
                return Err(Error::NoTargetAddr);
            }

            let port = istream.read_u16().await
                .map_err(Error::Read)?;

            //Pull address from load balancer
            let mut take_v4 = false;
            let mut take_v6 = false;
            for addr in &addrlist {
                match addr {
                    IpAddr::V4(_) => take_v4 = true,
                    IpAddr::V6(_) => take_v6 = true,
                }
            }

            let guard = balancer.take(take_v4, take_v6)
                .ok_or(Error::NoSuitableSourceAddr)?;

            //Create socket
            let source_addr = SocketAddr::from((guard.get_addr(), 0)); 
            log::debug!("{}: connecting from {}", client_addr, source_addr);
            let socket = if source_addr.is_ipv4() { 
                TcpSocket::new_v4() 
            } else {
                TcpSocket::new_v6() 
            }.map_err(Error::Socket)?;
            socket.bind(source_addr).map_err(Error::Bind)?;

            //Connect
            let target_addr = addrlist.into_iter()
                .find(|x| x.is_ipv4() == guard.get_addr().is_ipv4())
                .unwrap(); //< Should not fail
            let target_sockaddr = SocketAddr::from((target_addr, port));
            let target_stream = socket.connect(target_sockaddr).await
                .map_err(Error::Connect)?;

            Ok(target_stream)
        }.await;

        log::debug!("{}: Connection result: {:?}",
                    client_addr, maybe_target_stream);

        //Decide the response code for the request
        let reply_code : ReplyCode = match &maybe_target_stream {
            Ok(_) => ReplyCode::Success,
            Err(Error::UnsupportedCommand(_)) => ReplyCode::CommandNotSupported,
            Err(Error::NoSuitableSourceAddr) => ReplyCode::AddressTypeNotSupported,
            Err(Error::NameResolution(_)) => ReplyCode::GeneralSocksServerFailure,
            Err(Error::Connect(e)) => {
                use std::io::ErrorKind;
                match e.kind() {
                    //TODO: Why are standard errors still unstable?
                    //      Find an alternative, or wait for stabilization.
                    /*
                    ErrorKind::NetworkUnreachable => ReplyCode::NetworkUnreachable,
                    ErrorKind::HostUnreachable => ReplyCode::HostUnreachable,
                    */
                    ErrorKind::ConnectionRefused => ReplyCode::ConnectionRefused,
                    ErrorKind::TimedOut => ReplyCode::TTLExpired,
                    _ => ReplyCode::GeneralSocksServerFailure,
                }
            },
            _ => ReplyCode::GeneralSocksServerFailure,
        };

        //Send response
        let mut resp : Vec<u8> = vec![5, reply_code as u8, 0];
        let bind_addr = match maybe_target_stream.as_ref()
            .map(|s| s.local_addr()) {
            Ok(Ok(addr)) => addr,
            _ => SocketAddr::new(Ipv4Addr::UNSPECIFIED.into(), 0),
        };
        match bind_addr {
            SocketAddr::V4(addr) => {
                resp.push(AddrType::IPv4 as u8);
                resp.extend(addr.ip().octets());
            },
            SocketAddr::V6(addr) => {
                resp.push(AddrType::IPv6 as u8);
                resp.extend(addr.ip().octets());
            },
        }
        resp.extend(bind_addr.port().to_be_bytes());

        ostream.write_all(&resp).await
            .map_err(Error::Write)?;

        let target_stream = match maybe_target_stream {
            Ok(x) => x,
            Err(e) => {
                ostream.shutdown().await
                    .map_err(Error::Write)?;
                return Err(e);
            }
        };
        let (mut target_istream, mut target_ostream) 
            = target_stream.into_split();

        log::debug!("{}: Established", client_addr);

        //Transfer data between stream and target_stream
        let send_fut = tokio::io::copy_buf(&mut istream, &mut target_ostream);
        let recv_fut = tokio::io::copy(&mut target_istream, &mut ostream);
        futures::try_join!(send_fut, recv_fut)
            .map_err(Error::Copy)?;

        Ok(())
    }.await;
    if let Err(e) = result {
        log::warn!("{}: Error: {:?}", client_addr, e);
    }
}

#[cfg(test)]
mod test {
    use super::*;

    use std::marker::Unpin;

    use std::str::FromStr;
    use std::net::Ipv6Addr;

    use tokio::net::TcpListener;

    async fn start_echo_server(bind_addr : IpAddr) -> SocketAddr {
        let socket = TcpListener::bind((bind_addr, 0)).await
            .expect("echo_server bind");
        let addr = socket.local_addr().expect("echo_server getsockname");
        tokio::spawn(async move {
            while let Ok((client, _)) = socket.accept().await {
                tokio::spawn(async move {
                    let (mut client_read, mut client_write) 
                        = tokio::io::split(client);
                    tokio::io::copy(&mut client_read, &mut client_write).await
                        .expect("echo_server copy");
                });
            }
        });

        addr
    }

    async fn test_echo_server(mut conn : impl AsyncRead + AsyncWrite + Unpin) {
        let msg = [1_u8, 2, 3, 4];
        conn.write_all(&msg).await.expect("send"); 
        let mut resp = [0_u8; 4];
        conn.read_exact(&mut resp).await.expect("recv");
        assert_eq!(msg, resp);
    }

    async fn start_session(addr : IpAddr) -> impl AsyncRead + AsyncWrite {
        drop(env_logger::try_init());

        let (mut socks, socks_server) = tokio::io::duplex(256);
        let balancer = Balancer::new([addr].into_iter().collect());
        tokio::spawn(enter(socks_server, 
                           SocketAddr::from_str("127.0.0.1:1081").unwrap(),
                           balancer));

        socks.write_all(&[5_u8, 1, 0]).await.expect("handshake");
        let mut resp = [0_u8; 2];
        socks.read_exact(&mut resp).await.expect("handshake response");
        assert_eq!(resp, [5, 0]);

        socks
    }

    #[tokio::test]
    async fn test_ipv4() {
        let mut socks = start_session(Ipv4Addr::LOCALHOST.into()).await;
        let server_addr = start_echo_server(Ipv4Addr::LOCALHOST.into()).await;

        let mut msg = vec![5_u8, 1, 0, 1];
        msg.extend(Ipv4Addr::LOCALHOST.octets());
        msg.extend(server_addr.port().to_be_bytes());
        socks.write_all(&msg).await.expect("request");
        let mut resp = [0_u8; 10];
        socks.read_exact(&mut resp).await.expect("response");
        assert_eq!(&resp[0..4], &[5, 0, 0, 1]);

        test_echo_server(socks).await;
    }

    #[tokio::test]
    async fn test_ipv4_domain() {
        let mut socks = start_session(Ipv4Addr::LOCALHOST.into()).await;
        let server_addr = start_echo_server(Ipv4Addr::LOCALHOST.into()).await;

        let hostname = "localhost".as_bytes();
        let mut msg = vec![5_u8, 1, 0, 3, hostname.len() as u8];
        msg.extend(hostname);
        msg.extend(server_addr.port().to_be_bytes());
        socks.write_all(&msg).await.expect("request");
        let mut resp = [0_u8; 10];
        socks.read_exact(&mut resp).await.expect("response");
        assert_eq!(&resp[0..4], &[5, 0, 0, 1]);

        test_echo_server(socks).await;
    }

    #[tokio::test]
    async fn test_ipv6() {
        let mut socks = start_session(Ipv6Addr::LOCALHOST.into()).await;
        let server_addr = start_echo_server(Ipv6Addr::LOCALHOST.into()).await;

        let mut msg = vec![5_u8, 1, 0, 4];
        msg.extend(Ipv6Addr::LOCALHOST.octets());
        msg.extend(server_addr.port().to_be_bytes());
        socks.write_all(&msg).await.expect("request");
        let mut resp = [0_u8; 22];
        socks.read_exact(&mut resp).await.expect("response");
        assert_eq!(&resp[0..4], &[5, 0, 0, 4]);

        test_echo_server(socks).await;
    }

    #[tokio::test]
    async fn test_ipv6_domain() {
        let mut socks = start_session(Ipv6Addr::LOCALHOST.into()).await;
        let server_addr = start_echo_server(Ipv6Addr::LOCALHOST.into()).await;

        let hostname = "localhost".as_bytes();
        let mut msg = vec![5_u8, 1, 0, 3, hostname.len() as u8];
        msg.extend(hostname);
        msg.extend(server_addr.port().to_be_bytes());
        socks.write_all(&msg).await.expect("request");
        let mut resp = [0_u8; 22];
        socks.read_exact(&mut resp).await.expect("response");
        assert_eq!(&resp[0..4], &[5, 0, 0, 4]);

        test_echo_server(socks).await;
    }
}

