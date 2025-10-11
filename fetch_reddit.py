#!/usr/bin/env python3
import urllib.request
import json
import ssl

# Create SSL context
ctx = ssl.create_default_context()
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE

# The share URL needs to be resolved first
share_url = "https://www.reddit.com/r/esp32/s/4wcqmmjSZM"
headers = {
    'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
}

try:
    # First, follow the redirect to get the actual post URL
    req = urllib.request.Request(share_url, headers=headers)
    with urllib.request.urlopen(req, context=ctx, timeout=10) as response:
        actual_url = response.geturl()
        print(f"Actual URL: {actual_url}")
        
        # Now fetch JSON from actual URL
        json_url = actual_url + ".json"
        req = urllib.request.Request(json_url, headers=headers)
        with urllib.request.urlopen(req, context=ctx, timeout=10) as json_response:
            data = json.loads(json_response.read().decode())
            
            # Extract post information
            post = data[0]['data']['children'][0]['data']
            
            print(f"\nTitle: {post['title']}")
            print(f"Author: u/{post['author']}")
            print(f"Subreddit: r/{post['subreddit']}")
            
            # Check for external URL or self post
            if 'url' in post and not post['url'].startswith('https://www.reddit.com'):
                print(f"Project URL: {post['url']}")
            else:
                print(f"Reddit Post: https://www.reddit.com{post['permalink']}")
            
            if 'selftext' in post and post['selftext']:
                print(f"\nDescription: {post['selftext'][:500]}")
                
except Exception as e:
    print(f"Error: {type(e).__name__}: {e}")
    print("\nCould not automatically fetch Reddit post.")
    print("Please provide the project details manually.")
