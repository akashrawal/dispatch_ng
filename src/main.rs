/* main.rs
 * Root module
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


mod ord;
mod balance_heap;
mod balancer;
mod session;

use std::borrow::Cow;

use std::str::FromStr;

use std::net::IpAddr;

use clap::Parser;

use futures::prelude::*;
use futures::stream::FuturesUnordered;

use tokio::task::JoinHandle;
use tokio::net::TcpListener;

use balancer::Balancer;

#[derive(Debug, Parser)]
struct Args {
    /// Number of threads to use in the Async runtime. Default value of 1
    /// works well in all cases, unless you have multiple 10G or above 
    /// connections for some reason... Just leave it at 1. 
    #[arg(long, default_value_t = 1)]
    threads : usize, 

    /// Addresses to offer SOCKS5 proxy. If not specified, it listens on 
    /// localhost. 
    #[arg(short, long)]
    listen : Vec<Cow<'static, str>>,

    /// List of addresses to load-balance across.
    addrs : Vec<String>, 
}

//Async entry point
async fn async_main(args : Args) {
    //Default listening addresses
    let listen_addrs = if args.listen.is_empty() {
        vec!["127.0.0.1:1080".into(), "[::1]:1080".into()]    
    } else {
        args.listen
    };

    enum ListenErr {
        Bind(std::io::Error),
        Accept(std::io::Error),
    }

    //There should be atleast one address
    if args.addrs.is_empty() {
        panic!("No IP addresses provided for load balancing");
    }

    //Create the load balancer
    let mut addrs = Vec::<(IpAddr, usize)>::new();
    for addr in args.addrs.into_iter() {
        let parseres : Result<(IpAddr, usize), String> = (|| {
            let mut iter = addr.split("@");
            let addr_slice = iter.next()
                .ok_or(String::from("Unable to extract IP address"))?;
            let ipaddr = IpAddr::from_str(addr_slice)
                .map_err(|e| e.to_string())?;

            let metric = match iter.next() {
                Some(v) => usize::from_str(v).map_err(|e| e.to_string())?,
                None => 1
            };

            if metric < 1 {
                return Err("Metric should be atleast 1".into());
            }

            if iter.next().is_some() {
                return Err("There should be only one @ in an address to \
indicate metric".into());
            }

            Ok(( ipaddr, metric ))
        })();
        addrs.push(parseres.unwrap_or_else(|e| {
            panic!("Unable to parse address {}: {}", addr, e) 
        }));
    }
    let balancer = Balancer::new(addrs.into_iter().collect());

    //Start listening tasks
    let mut listeners : FuturesUnordered<JoinHandle<()>> 
        = listen_addrs.into_iter()
        .map( |addr| tokio::spawn(listen(addr, balancer.clone())) )
        .collect();

    while let Some(result) = listeners.next().await {
        match result {
            Ok(_) => (),
            Err(e) => panic!("Unable to wait for listeners: {}", e),
        }
    }

    panic!("No addresses available for listening");
}

//Listener task
async fn listen(addr : Cow<'static, str>, balancer : Balancer) {
    let listener = TcpListener::bind(addr.as_ref()).await
        .unwrap_or_else(|e| {
            panic!("Unable to bind SOCKS5 listener at address {}: {}",
                   addr, e)
        });

    loop {
        let (stream, remote_addr) = listener.accept()
            .await
            .expect("Unable to accept new connection");
    
        tokio::spawn(session::enter(stream, remote_addr, balancer.clone()));
    }
}


//Minimal main function
fn main() {
    env_logger::init();

    //Read the argument vector
    let args = Args::parse();
    log::debug!("Args: {:?}", args);

    //Create and launch runtime
    let runtime = if args.threads == 1 {
        tokio::runtime::Builder::new_current_thread()
            .enable_io()
            .build()
    } else {
        tokio::runtime::Builder::new_multi_thread()
            .worker_threads(args.threads)
            .enable_io()
            .build()
    };
    let runtime = runtime.expect("Unable to create new runtime");
    runtime.block_on(async_main(args));
}
