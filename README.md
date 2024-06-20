# Raw socket을 이용한 L4 Load Balancer 제작 프로젝트
2024년도 2학기 IoT 실습 중간 프로젝트

## 컴파일 방법
프로젝트를 컴파일하려면 각 디렉토리(server, client, lb)로 이동한 후 `make` 명령어를 실행하세요.

```
# server 컴파일
cd server/
make

# client 컴파일
cd client/ 
make

# lb 컴파일
cd lb/
make
```

## 실행 방법

### 서버
서버를 실행하려면 다음 명령어를 사용하세요:

```
./server.out <src_ip> <src_port>
```

`<src_ip>`를 원하는 소스 IP 주소로, `<src_port>`를 원하는 소스 포트 번호로 대체하세요.

### 로드밸런서
로드밸런서를 실행하려면 다음 명령어를 사용하세요:

```
./lb.out <Source IP> <Source Port> <LB_ALGORITHM>
```

`<Source IP>`를 원하는 소스 IP 주소로, `<Source Port>`를 원하는 소스 포트 번호로, `<LB_ALGORITHM>`을 부하 분산 알고리즘으로 대체하세요.

0: ROUND_ROBIN  
1: LEAST_CONNECTION  
2: RESOURCE_BASED  

### 클라이언트
클라이언트를 실행하려면 다음 명령어를 사용하세요:

```
./client.out <Source IP> <Destination IP> <Destination Port>
```

`<Source IP>`를 원하는 소스 IP 주소로, `<Destination IP>`를 원하는 대상 IP 주소로, `<Destination Port>`를 원하는 대상 포트 번호로 대체하세요. 