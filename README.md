# Peer-to-Peer-Server

<h2>Overview</h2>
<p>This project is a simple implementation of a peer-to-peer server in C, designed to share and download files among connected servers. The server reads its configuration from a config.txt file, which contains the IP addresses and ports of its neighboring servers. The project also provides an output file containing log information.</p>

<h2>Features</h2>
<ul>
  <li><strong>Configurable:</strong> The server is configured using the config.txt file, allowing easy customization of neighbor connections.</li>
   <li><strong>File Search:</strong> The server can search for files within its network of connected servers.</li>
   <li><strong>File Download:</strong> Users can request the server to download a specific file from its neighbors.</li>
   <li><strong>Log Output:</strong> The server generates an output file containing log information.</li>
</ul>

<h2>Prerequisites</h2>
<ul>
  <li>C Compiler (e.g., GCC)</li>
  <li>POSIX-compatible operating system (Linux recommended)</li>
</ul>

<h2>Installation Steps</h2>
<ol>
  <li>Download the Source Code:</li>
  <ul>
    <li>Download the peer_to_peer_server.c file to your server.</li>
  </ul>
  <li>Create Configuration File:</li>
  <ul>
    <li>Write the config.txt file with the following format:</li>
    <pre>
    &lt;ip_address_1&gt; &lt;port_1&gt;
    &lt;ip_address_2&gt; &lt;port_2&gt;
    </pre>
      <p>Specify the IP addresses and ports of the neighboring servers, with each entry on a new line.</p>
    <li>Example Configuration (config.txt)</li>
    <pre>
    192.168.1.2 5001
    192.168.1.3 5002
    192.168.1.4 5003
    </pre>
  </ul>
  <li>Compile the Server:</li>
        <ul>
          <li>Open a terminal and run the following command to compile the server:</li>
          <strong>gcc -o peer_to_peer peer_to_peer.c</strong>
        </ul>
  <li>Run the Server:</li>
        <ul>
          <li>Execute the compiled server with the config.txt file as an argument:</li>
          <strong>./peer_to_peer config.txt</strong>
        </ul>
</ol>
