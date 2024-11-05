from flask import Flask, request, send_file
import io

app = Flask(__name__)

@app.route('/compile', methods=['POST'])
def upload_and_return_file():
    if 'file' not in request.files:
        return "No file part", 400
    file = request.files['file']
    file_content = file.read()  # Read the uploaded file content
    file_like = io.BytesIO(file_content)  # Create an in-memory stream for the response
    file_like.seek(0)
    return send_file(
        file_like,
        as_attachment=True,
        download_name=file.filename,
        mimetype=file.mimetype or 'application/octet-stream'
    )

if __name__ == '__main__':
    app.run(debug=True)
