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
        senha TEXT NOT NULL,
        avatar TEXT
    )''')
    conn.commit()
    # ensure avatar column exists (for older DBs)
    try:
        c.execute("PRAGMA table_info(usuarios)")
        cols = [r[1] for r in c.fetchall()]
        if 'avatar' not in cols:
            c.execute('ALTER TABLE usuarios ADD COLUMN avatar TEXT')
            conn.commit()
    except Exception:
        pass
    conn.close()


@app.route('/api/getProfile', methods=['GET'])
def get_profile():
    email = request.args.get('email')
    if not email:
        return jsonify({'success': False, 'message': 'E-mail ausente'}), 400
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('SELECT id, nome, sobrenome, email, avatar FROM usuarios WHERE email = ?', (email,))
    row = c.fetchone()
    conn.close()
    if not row:
        return jsonify({'success': False, 'message': 'Usuário não encontrado'}), 404
    user = {'id': row[0], 'nome': row[1], 'sobrenome': row[2], 'email': row[3], 'avatar': row[4]}
    return jsonify({'success': True, 'user': user}), 200


@app.route('/api/updateProfile', methods=['POST'])
def update_profile():
    data = request.json or {}
    email = data.get('email')
    if not email:
        return jsonify({'success': False, 'message': 'E-mail ausente'}), 400
    nome = data.get('nome')
    sobrenome = data.get('sobrenome')
    senha = data.get('senha')
    avatar = data.get('avatar')
    # Build update dynamically
    fields = []
    params = []
    if nome is not None:
        fields.append('nome = ?'); params.append(nome)
    if sobrenome is not None:
        fields.append('sobrenome = ?'); params.append(sobrenome)
    if senha is not None and senha != '':
        fields.append('senha = ?'); params.append(senha)
    if avatar is not None:
        fields.append('avatar = ?'); params.append(avatar)
    if not fields:
        return jsonify({'success': False, 'message': 'Nenhum campo para atualizar.'}), 400
    params.append(email)
    sql = 'UPDATE usuarios SET ' + ', '.join(fields) + ' WHERE email = ?'
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('SELECT id FROM usuarios WHERE email = ?', (email,))
    if not c.fetchone():
        conn.close()
        return jsonify({'success': False, 'message': 'Usuário não encontrado.'}), 404
    c.execute(sql, tuple(params))
    conn.commit()
    conn.close()
    return jsonify({'success': True, 'message': 'Perfil atualizado.'}), 200

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
        # user tuple: (id, nome, sobrenome, email, senha)
        user_obj = {
            'id': user[0],
            'nome': user[1],
            'sobrenome': user[2],
            'email': user[3]
        }
        return jsonify({'success': True, 'message': 'Login realizado com sucesso!', 'user': user_obj}), 200
    else:
        return jsonify({'success': False, 'message': 'E-mail ou senha incorretos.'}), 401

if __name__ == '__main__':
    init_db()
    app.run(debug=True)
