from flask import Flask, request, jsonify, Response
from google import genai
from PIL import Image
import json
import io
import time
import os

# =========================================================
# Smart Bin - Python AI Server
# מקבל תמונה מה-ESP32-CAM,
# שומר את התמונה לבדיקה,
# שולח ל-Gemini,
# מחזיר JSON דחוס בשורה אחת ל-ESP32-CAM
# =========================================================

# חשוב:
# המפתח שלך נחשף בצ'אט.
# אחרי הבדיקות תבטל אותו בגוגל ותיצור חדש.
API_KEY = "AIzaSyAK_zQxezhx82y8Lo4Cr95ZGqPaWQPJsw8"

GEMINI_MODEL = "gemini-2.5-flash"

MAX_GEMINI_RETRIES = 3
RETRY_DELAY_SECONDS = 5

LAST_IMAGE_PATH = "last_received_image.jpg"

app = Flask(__name__)
client = genai.Client(api_key=API_KEY)


RECYCLING_PROMPT = """
You are a recycling sorting system.

Your job is to classify the main object in the image into exactly one of these categories:
paper
plastic
metal
unknown

Important rules:
- There is only one main object in the image.
- Classify by material, not by object name.
- A plastic bottle cap is plastic.
- A plastic bottle is plastic.
- A plastic wrapper is plastic.
- A coin, aluminum foil, metal cap, or paper clip is metal.
- Cardboard, paper, receipt, package carton, or paper packaging is paper.
- If the image is too dark, blurry, empty, or unclear, return unknown.
- Return only valid JSON.
- Do not write anything outside the JSON.

JSON format:
{
  "category": "paper/plastic/metal/unknown",
  "confidence": 0-100,
  "reason": "short reason"
}
"""


# =========================================================
# Compact JSON Response
# מחזיר JSON בשורה אחת, בלי רווחים
# זה חשוב כדי שה-ESP32 הראשי יצליח לקרוא category
# =========================================================
def compact_json_response(data):
    return Response(
        json.dumps(data, separators=(",", ":")),
        status=200,
        mimetype="application/json"
    )


def build_response(category, confidence, reason):
    return {
        "success": True,
        "category": category,
        "confidence": confidence,
        "reason": reason
    }


def unknown(reason):
    return {
        "category": "unknown",
        "confidence": 0,
        "reason": reason
    }


def clean_gemini_json(raw_text):
    print("\n========== RAW GEMINI RESPONSE ==========")
    print(raw_text)
    print("=========================================\n")

    clean_text = raw_text.strip()

    if clean_text.startswith("```"):
        clean_text = clean_text.replace("```json", "")
        clean_text = clean_text.replace("```", "")
        clean_text = clean_text.strip()

    return json.loads(clean_text)


def validate_result(result):
    allowed_categories = ["paper", "plastic", "metal", "unknown"]

    category = result.get("category", "unknown")
    confidence = result.get("confidence", 0)
    reason = result.get("reason", "")

    try:
        confidence = int(confidence)
    except:
        return unknown("Invalid confidence returned by Gemini.")

    if category not in allowed_categories:
        return unknown("Invalid category returned by Gemini.")

    if confidence < 0 or confidence > 100:
        return unknown("Confidence out of range.")

    if not reason:
        reason = "No reason provided."

    return {
        "category": category,
        "confidence": confidence,
        "reason": reason
    }


def classify_image_with_gemini(image):
    last_error = None

    for attempt in range(1, MAX_GEMINI_RETRIES + 1):
        try:
            print(f"\nGemini attempt {attempt}/{MAX_GEMINI_RETRIES} with {GEMINI_MODEL}")

            response = client.models.generate_content(
                model=GEMINI_MODEL,
                contents=[RECYCLING_PROMPT, image],
            )

            if not response.text:
                print("Gemini returned empty response.")
                last_error = "Empty Gemini response"
            else:
                parsed_result = clean_gemini_json(response.text)
                validated_result = validate_result(parsed_result)

                print("Validated Gemini result:")
                print(validated_result)
                return validated_result

        except Exception as error:
            last_error = error
            print(f"Gemini failed on attempt {attempt}:")
            print(error)

        if attempt < MAX_GEMINI_RETRIES:
            time.sleep(RETRY_DELAY_SECONDS)

    print(f"Gemini failed after {MAX_GEMINI_RETRIES} attempts.")
    print("Last error:")
    print(last_error)

    return unknown(f"Gemini unavailable after {MAX_GEMINI_RETRIES} attempts.")


def save_received_image(image_bytes):
    with open(LAST_IMAGE_PATH, "wb") as file:
        file.write(image_bytes)

    absolute_path = os.path.abspath(LAST_IMAGE_PATH)

    print("Saved received image:")
    print(absolute_path)
    print("Image size in bytes:")
    print(len(image_bytes))


@app.route("/", methods=["GET"])
def home():
    return compact_json_response({
        "status": "Smart Bin AI Server is running",
        "use": "POST /classify with image field named image",
        "last_image": LAST_IMAGE_PATH
    })


@app.route("/classify", methods=["POST"])
def classify():
    try:
        print("\n\n========== NEW /classify REQUEST ==========")

        if "image" not in request.files:
            print("No image field found in request.")

            return compact_json_response(build_response(
                "unknown",
                0,
                "No image file found."
            ))

        image_file = request.files["image"]
        image_bytes = image_file.read()

        save_received_image(image_bytes)

        if len(image_bytes) == 0:
            print("Image file is empty.")

            return compact_json_response(build_response(
                "unknown",
                0,
                "Image file is empty."
            ))

        try:
            image = Image.open(io.BytesIO(image_bytes))
            image = image.convert("RGB")

            print("Image opened successfully.")
            print("Image size:")
            print(image.size)

        except Exception as image_error:
            print("Failed to open image:")
            print(image_error)

            return compact_json_response(build_response(
                "unknown",
                0,
                "Server could not open received image."
            ))

        result = classify_image_with_gemini(image)

        final_response = build_response(
            result["category"],
            result["confidence"],
            result["reason"]
        )

        print("\n========== FINAL SERVER RESPONSE ==========")
        print(final_response)
        print("===========================================\n")

        return compact_json_response(final_response)

    except Exception as error:
        print("Server error:")
        print(error)

        return compact_json_response(build_response(
            "unknown",
            0,
            "Server error while classifying image."
        ))


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
