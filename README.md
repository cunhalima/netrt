netrt
=====

Este programa faz o roteamento de pacotes UDP de modo confiável.

1 Execução
----------
A sintaxe da linha de comando é a seguinte:

    ./net <id-do-roteador> [ -e percentual_de_erro_simulado ]

Assim, cada nó (roteador) corresponde a um processo (uma instância do programa "net").
Ao executar o programa, será exibido na tela a mensagem "node number = <n>" onde
<n> é o id do nó fornecido na linha de comando.
Em seguida, o programa aguardará que o usuário escreva um comando na entrada padrão (teclado).
Há dois possíveis comandos: o comando para encerrar o programa e o comando para enviar uma mensagem.
Para encerrar o programa, basta digitar qualquer texto que se inicie pela letra "q".
Para enviar uma mensagem, deve-se escrever o número do nó de destino (com dois dígitos, por exemplo: 01, 02, 10, 15 etc).
Após, deve-se deixar um espaço em branco então escrever a mensagem que se deseja enviar ao nó de destino.

2 Saídas na tela
----------------

Durante a execução do programa, são mostradas algumas saídas na tela:
a) RECV <...>
    Esta saída indica que o roteador recebeu dados de algum outro roteador
b) SEND <...>
    Esta saída indica que o roteador enviou dados para algum outro roteador
c) <número>: <...>
    Esta saída indica que o roteador recebeu uma mensagem como destinatário final
d) ME: <...>
    Esta saída indica que a respectiva mensagem enviada pelo roteador foi recebida com sucesso.
