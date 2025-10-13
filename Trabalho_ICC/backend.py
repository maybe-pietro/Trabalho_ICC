from flask import Flask, request, jsonify
from flask_cors import CORS
import sqlite3

app = Flask(__name__)
CORS(app)

# Banco de dados SQLite simples
DB_NAME = 'usuarios.db'

def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS usuarios (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        nome TEXT NOT NULL,
        sobrenome TEXT NOT NULL,
        email TEXT NOT NULL UNIQUE,
        senha TEXT NOT NULL
    )''')
    conn.commit()
    conn.close()

@app.route('/api/registrar', methods=['POST'])
def registrar():
    data = request.json
    nome = data.get('nome')
    sobrenome = data.get('sobrenome')
    email = data.get('email')
    senha = data.get('senha')
    if not all([nome, sobrenome, email, senha]):
        return jsonify({'success': False, 'message': 'Preencha todos os campos.'}), 400
    try:
        conn = sqlite3.connect(DB_NAME)
        c = conn.cursor()
        c.execute('INSERT INTO usuarios (nome, sobrenome, email, senha) VALUES (?, ?, ?, ?)',
                  (nome, sobrenome, email, senha))
        conn.commit()
        conn.close()
        return jsonify({'success': True, 'message': 'Usuário cadastrado com sucesso!'}), 201
    except sqlite3.IntegrityError:
        return jsonify({'success': False, 'message': 'E-mail já cadastrado.'}), 409

@app.route('/api/login', methods=['POST'])
def login():
    data = request.json
    email = data.get('email')
    senha = data.get('senha')
    if not all([email, senha]):
        return jsonify({'success': False, 'message': 'Preencha todos os campos.'}), 400
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('SELECT * FROM usuarios WHERE email = ? AND senha = ?', (email, senha))
    user = c.fetchone()
    conn.close()
    if user:
        return jsonify({'success': True, 'message': 'Login realizado com sucesso!'}), 200
    else:
        return jsonify({'success': False, 'message': 'E-mail ou senha incorretos.'}), 401

if __name__ == '__main__':
    init_db()
    app.run(debug=True)
