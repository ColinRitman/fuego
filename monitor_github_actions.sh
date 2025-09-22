#!/bin/bash

# Enhanced GitHub Actions Monitor Script
# Monitors all workflow builds and provides detailed status

REPO="ColinRitman/fuego"
WORKFLOWS=("build.yml" "appimage.yml" "docker.yml" "raspberry-pi.yml" "test-dynamic-supply.yml" "testnet.yml" "termux.yml" "release.yml")
CHECK_INTERVAL=120  # 2 minutes

echo "🔍 Enhanced GitHub Actions Monitor for $REPO"
echo "📋 Monitoring workflows: ${WORKFLOWS[*]}"
echo "⏰ Checking every $(($CHECK_INTERVAL / 60)) minutes..."
echo "=================================="

# Function to check workflow status
check_workflow() {
    local workflow=$1
    local workflow_name=$(echo $workflow | sed 's/.yml//')
    
    echo ""
    echo "🔧 Checking $workflow_name..."
    
    # Get the latest workflow run
    local response=$(curl -s "https://api.github.com/repos/$REPO/actions/workflows/$workflow/runs?per_page=1")
    local status=$(echo "$response" | jq -r '.workflow_runs[0].status // "no_runs"')
    local conclusion=$(echo "$response" | jq -r '.workflow_runs[0].conclusion // "unknown"')
    local html_url=$(echo "$response" | jq -r '.workflow_runs[0].html_url // "unknown"')
    local run_number=$(echo "$response" | jq -r '.workflow_runs[0].run_number // "unknown"')
    local created_at=$(echo "$response" | jq -r '.workflow_runs[0].created_at // "unknown"')
    
    if [ "$status" = "no_runs" ]; then
        echo "  ℹ️  No recent runs found"
        return 0
    fi
    
    echo "  📊 Run #$run_number - Status: $status"
    echo "  🎯 Conclusion: $conclusion"
    echo "  🕐 Created: $created_at"
    echo "  🔗 URL: $html_url"
    
    # Get job details if available
    local run_id=$(echo "$response" | jq -r '.workflow_runs[0].id // "unknown"')
    if [ "$run_id" != "unknown" ] && [ "$run_id" != "null" ]; then
        local jobs_response=$(curl -s "https://api.github.com/repos/$REPO/actions/runs/$run_id/jobs")
        local job_count=$(echo "$jobs_response" | jq -r '.jobs | length')
        
        if [ "$job_count" -gt 0 ]; then
            echo "  📋 Jobs ($job_count):"
            echo "$jobs_response" | jq -r '.jobs[] | "    \(.name): \(.status) - \(.conclusion // "in_progress")"' 2>/dev/null || echo "    Unable to fetch job details"
        fi
    fi
    
    # Status indicators
    case "$status" in
        "completed")
            case "$conclusion" in
                "success")
                    echo "  ✅ Build successful!"
                    return 0
                    ;;
                "failure")
                    echo "  ❌ Build failed!"
                    return 1
                    ;;
                "cancelled")
                    echo "  ⏹️ Build was cancelled"
                    return 1
                    ;;
                *)
                    echo "  ⚠️ Build completed with status: $conclusion"
                    return 1
                    ;;
            esac
            ;;
        "in_progress")
            echo "  🔄 Build in progress..."
            return 2
            ;;
        "queued")
            echo "  ⏳ Build queued..."
            return 2
            ;;
        *)
            echo "  ❓ Unknown status: $status"
            return 1
            ;;
    esac
}

# Function to show summary
show_summary() {
    local successful=0
    local failed=0
    local in_progress=0
    
    echo ""
    echo "📈 WORKFLOW SUMMARY"
    echo "==================="
    
    for workflow in "${WORKFLOWS[@]}"; do
        check_workflow "$workflow"
        case $? in
            0) ((successful++)) ;;
            1) ((failed++)) ;;
            2) ((in_progress++)) ;;
        esac
    done
    
    echo ""
    echo "📊 TOTALS:"
    echo "  ✅ Successful: $successful"
    echo "  ❌ Failed: $failed"
    echo "  🔄 In Progress: $in_progress"
    echo "  📝 Total: ${#WORKFLOWS[@]}"
    
    if [ $failed -eq 0 ] && [ $in_progress -eq 0 ]; then
        echo ""
        echo "🎉 ALL WORKFLOWS GREEN! 🎉"
        return 0
    elif [ $in_progress -gt 0 ]; then
        echo ""
        echo "⏳ Workflows still running..."
        return 1
    else
        echo ""
        echo "❌ Some workflows failed"
        return 1
    fi
}

# Main monitoring loop
while true; do
    echo ""
    echo "🕐 $(date '+%Y-%m-%d %H:%M:%S') - Checking all workflows..."
    echo "================================================="
    
    if show_summary; then
        echo ""
        echo "🎊 SUCCESS! All workflows are green! 🎊"
        echo "Monitoring complete."
        break
    fi
    
    echo ""
    echo "⏰ Waiting $(($CHECK_INTERVAL / 60)) minutes before next check..."
    sleep $CHECK_INTERVAL
done
