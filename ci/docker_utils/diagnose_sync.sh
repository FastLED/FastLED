#!/bin/bash
# Diagnostic script to check if rsync is detecting spurious changes
# Run this inside the Docker container to compare host vs container files

set -e

echo "=== Rsync Sync Diagnostic ==="
echo ""
echo "Checking for differences between /host and /fastled..."
echo ""

# Test directories
DIRS=("src" "examples" "ci")

for dir in "${DIRS[@]}"; do
    if [ ! -d "/host/$dir" ] || [ ! -d "/fastled/$dir" ]; then
        echo "⚠️  Skipping $dir (not found)"
        continue
    fi

    echo "📁 Checking $dir/..."

    # Run rsync in dry-run mode with checksum to see what would be copied
    # --no-t flag means timestamps won't affect the decision
    OUTPUT=$(rsync -a --no-t --checksum --dry-run --itemize-changes \
        --exclude='**/__pycache__' --exclude='.build/' --exclude='.pio/' --exclude='*.pyc' \
        "/host/$dir/" "/fastled/$dir/" 2>&1)

    # Count how many files would be updated
    CHANGED_FILES=$(echo "$OUTPUT" | grep -c "^>" || true)

    if [ $CHANGED_FILES -eq 0 ]; then
        echo "   ✓ No changes detected (checksums match)"
    else
        echo "   ⚠️  $CHANGED_FILES files would be synced:"
        echo "$OUTPUT" | grep "^>" | head -20

        # Check first changed file for line ending differences
        FIRST_FILE=$(echo "$OUTPUT" | grep "^>" | head -1 | awk '{print $2}')
        if [ -n "$FIRST_FILE" ] && [ -f "/host/$dir/$FIRST_FILE" ] && [ -f "/fastled/$dir/$FIRST_FILE" ]; then
            echo ""
            echo "   🔍 Analyzing first changed file: $FIRST_FILE"

            # Check for line ending differences
            HOST_CRLF=$(file "/host/$dir/$FIRST_FILE" | grep -c "CRLF" || true)
            CONTAINER_CRLF=$(file "/fastled/$dir/$FIRST_FILE" | grep -c "CRLF" || true)

            if [ $HOST_CRLF -ne $CONTAINER_CRLF ]; then
                echo "   ❌ LINE ENDING MISMATCH DETECTED!"
                echo "      Host: $(file "/host/$dir/$FIRST_FILE")"
                echo "      Container: $(file "/fastled/$dir/$FIRST_FILE")"
            else
                echo "   ℹ️  Line endings match - investigating other differences"

                # Show size difference
                HOST_SIZE=$(stat -c%s "/host/$dir/$FIRST_FILE" 2>/dev/null || stat -f%z "/host/$dir/$FIRST_FILE" 2>/dev/null)
                CONTAINER_SIZE=$(stat -c%s "/fastled/$dir/$FIRST_FILE" 2>/dev/null || stat -f%z "/fastled/$dir/$FIRST_FILE" 2>/dev/null)
                echo "      Host size: $HOST_SIZE bytes"
                echo "      Container size: $CONTAINER_SIZE bytes"

                # Show checksum difference
                echo "      Host MD5: $(md5sum "/host/$dir/$FIRST_FILE" | cut -d' ' -f1)"
                echo "      Container MD5: $(md5sum "/fastled/$dir/$FIRST_FILE" | cut -d' ' -f1)"
            fi
        fi
    fi
    echo ""
done

echo "=== Summary ==="
echo "This diagnostic checks if rsync would copy files between /host and /fastled"
echo "using the same flags as the actual sync (--checksum --no-t)"
echo ""
echo "If files show as changed despite not being modified, this indicates:"
echo "  • Line ending differences (CRLF vs LF)"
echo "  • Actual content differences"
echo "  • Checksum algorithm issues"
